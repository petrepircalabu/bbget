#ifndef HTTP_OUTBOUND_CONNECTION_HPP
#define HTTP_OUTBOUND_CONNECTION_HPP

#include "proxy.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/url/url.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <string_view>
#include <variant>

namespace bbget {
namespace http {
namespace outbound {

template <typename Derived, typename CreateConnectionFn> class connection {
public:
	connection(boost::asio::io_context& ioc, bbget::proxy::config&& proxy_config, int max_redirect,
	           const CreateConnectionFn& f)
	    : ioc_(ioc)
	    , proxy_config_(std::forward<bbget::proxy::config>(proxy_config))
	    , max_redirect_(max_redirect)
	    , f_(f)
	    , resolver_(boost::asio::make_strand(ioc))
	{
	}

	Derived& derived()
	{
		return static_cast<Derived&>(*this);
	}

	void start(std::string_view host, std::string_view port,
	           boost::beast::http::request<boost::beast::http::string_body>& req)
	{
		req_ = std::move(req);
		spdlog::debug("{} host: {}, port:{}", __func__, host, port);
		resolver_.async_resolve(host, port,
		                        boost::beast::bind_front_handler(&connection::on_resolve,
		                                                         derived().shared_from_this()));
	}

protected:
	void send_request()
	{

		spdlog::debug("{}", __func__);
		spdlog::debug("{}", req_);
		boost::beast::http::async_write(
		    derived().stream(), req_,
		    boost::beast::bind_front_handler(&connection::on_write, derived().shared_from_this()));
	}

private:
	void on_resolve(const boost::beast::error_code&              ec,
	                boost::asio::ip::tcp::resolver::results_type results)
	{
		spdlog::debug("{}", __func__);
		if (ec) {
			spdlog::error("{}: error {}", __func__, ec);
			return;
		}

		boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

		boost::beast::get_lowest_layer(derived().stream())
		    .async_connect(results,
		                   boost::beast::bind_front_handler(&connection::on_connect,
		                                                    derived().shared_from_this()));
	}

	void on_connect(const boost::beast::error_code& ec,
	                boost::asio::ip::tcp::resolver::results_type::endpoint_type)
	{
		spdlog::debug("{}", __func__);
		if (ec) {
			spdlog::error("{}: error {}", __func__, ec);
			return;
		}

		boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

		derived().hook_connected();
	}

	void on_write(const boost::beast::error_code& ec, const std::size_t bytes_transferred)
	{
		spdlog::debug("{}", __func__);
		if (ec) {
			spdlog::error("{}: error {}", __func__, ec);
			return;
		}

		boost::beast::http::async_read_header(
		    derived().stream(), buffer_, parser_,
		    boost::beast::bind_front_handler(&connection::on_read_header,
		                                     derived().shared_from_this()));
	}

	void on_read_header(boost::beast::error_code ec, std::size_t bytes_transferred)
	{
		spdlog::debug("{}", __func__);
		if (ec) {
			spdlog::error("{}: error {}", __func__, ec);
			return;
		}

		auto header = parser_.get();

		switch (header.result()) {
		case boost::beast::http::status::multiple_choices:
		case boost::beast::http::status::moved_permanently:
		case boost::beast::http::status::found:
		case boost::beast::http::status::see_other:
		case boost::beast::http::status::not_modified:
		case boost::beast::http::status::temporary_redirect:
		case boost::beast::http::status::permanent_redirect: {
			auto location = header.at(boost::beast::http::field::location);
			spdlog::debug("{}", header);
			if (max_redirect_ == 0) {
				spdlog::error("Redirect count exceeded");
				return;
			};
			spdlog::info("Redirecting to {}", location);
			f_(std::string(location), std::move(proxy_config_), max_redirect_ - 1);
			// NOTE: Existing connection is not valid past this point.
			return;
		}
		default:
			break;
		}

		boost::beast::http::async_read(
		    derived().stream(), buffer_, parser_,
		    boost::beast::bind_front_handler(&connection::on_read, derived().shared_from_this()));
	}

	void on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
	{
		spdlog::debug("{}", __func__);

		boost::beast::get_lowest_layer(derived().stream())
		    .socket()
		    .shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

		spdlog::debug("{}", parser_.release());

		// not_connected happens sometimes so don't bother reporting it.
		if (ec && ec != boost::beast::errc::not_connected) {
			spdlog::error("shutdown: {}", ec.message());
		}
	}

private:
	boost::asio::io_context&                                             ioc_;
	bbget::proxy::config&&                                               proxy_config_;
	int                                                                  max_redirect_;
	CreateConnectionFn                                                   f_;
	boost::asio::ip::tcp::resolver                                       resolver_;
	boost::beast::http::request<boost::beast::http::string_body>         req_;
	boost::beast::flat_buffer                                            buffer_;
	boost::beast::http::response_parser<boost::beast::http::string_body> parser_;
};

template <typename CreateConnectionFn>
class plain_connection
    : public connection<plain_connection<CreateConnectionFn>, CreateConnectionFn>,
      public std::enable_shared_from_this<plain_connection<CreateConnectionFn>> {
	using base_t = connection<plain_connection<CreateConnectionFn>, CreateConnectionFn>;

public:
	plain_connection(boost::asio::io_context& ioc, bbget::proxy::config&& proxy_config,
	                 int max_redirect, const CreateConnectionFn& f)
	    : base_t(ioc, std::forward<bbget::proxy::config>(proxy_config), max_redirect, f)
	    , stream_(boost::asio::make_strand(ioc))
	{
	}

	boost::beast::tcp_stream& stream()
	{
		return stream_;
	}

	void hook_connected()
	{
		spdlog::debug("Connection established");
		base_t::send_request();
	}

private:
	boost::beast::tcp_stream stream_;
};

template <typename CreateConnectionFn>
class ssl_connection : public connection<ssl_connection<CreateConnectionFn>, CreateConnectionFn>,
                       public std::enable_shared_from_this<ssl_connection<CreateConnectionFn>> {
	using base_t = connection<ssl_connection<CreateConnectionFn>, CreateConnectionFn>;

public:
	ssl_connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	               bbget::proxy::config&& proxy_config, int max_redirect,
	               const CreateConnectionFn& f)
	    : base_t(ioc, std::forward<bbget::proxy::config>(proxy_config), max_redirect, f)
	    , stream_(boost::asio::make_strand(ioc), ssl_ctx)
	{
	}

	boost::beast::ssl_stream<boost::beast::tcp_stream>& stream()
	{
		return stream_;
	}

	void hook_connected()
	{
		spdlog::debug("Connection established");
		stream_.async_handshake(boost::asio::ssl::stream_base::client,
		                        boost::beast::bind_front_handler(&ssl_connection::on_handshake,
		                                                         this->shared_from_this()));
	}

private:
	void on_handshake(const boost::beast::error_code ec)
	{
		spdlog::debug("{}", __func__);
		if (ec) {
			spdlog::error("{}: error {}", __func__, ec);
		}

		boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

		base_t::send_request();
	}

private:
	boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
};

class create_connection {
public:
	create_connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx)
	    : ioc_(ioc)
	    , ssl_ctx_(ssl_ctx)
	{
	}

	bool operator()(const std::string& url_string, bbget::proxy::config&& proxy_config,
	                int max_redirect)
	{
		auto res = boost::urls::parse_uri(url_string);
		if (res.has_error()) {
			spdlog::error("{}: error parsing URI {}: {}", __func__, url_string, res.error().what());
			return false;
		}

		boost::urls::url url = res.value();

		if (!url.has_scheme()) {
			if (url.has_port() && (url.port_number() == 443) || (url.port_number() == 8443))
				url.set_scheme(boost::urls::scheme::https);
			else
				url.set_scheme(boost::urls::scheme::http);
		}

		std::string host = proxy_config.enabled ? proxy_config.host : url.encoded_host();
		std::string port = proxy_config.enabled             ? proxy_config.port
		    : url.has_port()                                ? url.port()
		    : url.scheme_id() == boost::urls::scheme::https ? "443"
		                                                    : "80";

		boost::beast::http::request<boost::beast::http::string_body> req(
		    boost::beast::http::verb::get, proxy_config.enabled ? url.string() : url.encoded_path(),
		    11);
		req.set(boost::beast::http::field::host, url.encoded_host());
		req.set(boost::beast::http::field::user_agent, "BBGet");
		if (proxy_config.enabled && !proxy_config.auth.empty()) {
			req.set(boost::beast::http::field::proxy_authorization, "Basic: " + proxy_config.auth);
		}

		if (url.scheme_id() == boost::urls::scheme::http) {
			auto conn = std::make_shared<plain_connection<create_connection>>(
			    ioc_, std::move(proxy_config), max_redirect, *this);
			conn->start(host, port, std::move(req));
		} else if (url.scheme_id() == boost::urls::scheme::https) {
			auto conn = std::make_shared<ssl_connection<create_connection>>(
			    ioc_, ssl_ctx_, std::move(proxy_config), max_redirect, *this);
			conn->start(host, port, std::move(req));
		}

		return true;
	}

private:
	boost::asio::io_context&   ioc_;
	boost::asio::ssl::context& ssl_ctx_;
};

} // namespace outbound
} // namespace http
} // namespace bbget

#endif // HTTP_OUTBOUND_CONNECTION_HPP
