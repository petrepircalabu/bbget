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

template <typename Derived, typename Callback> class connection {
public:
	using callback_t = Callback;

	connection(boost::asio::io_context& ioc, int max_redirect, Callback&& cb, bool skip_connect)
	    : ioc_(ioc)
	    , max_redirect_(max_redirect)
	    , cb_(std::move(cb))
	    , resolver_(boost::asio::make_strand(ioc))
	    , skip_connect_(skip_connect)
	{
	}

	Derived& derived()
	{
		return static_cast<Derived&>(*this);
	}

	int max_redirect() const
	{
		return max_redirect_;
	}

	void start(std::string_view host, std::string_view port,
	           boost::beast::http::request<boost::beast::http::string_body>&& req)
	{
		req_ = std::move(req);

		if (skip_connect_) {
			return derived().hook_connected();
		}

		spdlog::debug("{} host: {}, port:{}", __func__, host, port);
		resolver_.async_resolve(host, port,
		                        boost::beast::bind_front_handler(&connection::on_resolve,
		                                                         derived().shared_from_this()));
	}

	const boost::beast::http::request<boost::beast::http::string_body>& get_request() const
	{
		return req_;
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

		if (cb_(derived().shared_from_this(), std::move(parser_.release())))
			return;

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
	int                                                                  max_redirect_;
	callback_t                                                           cb_;
	bool                                                                 skip_connect_ {false};
	boost::asio::ip::tcp::resolver                                       resolver_;
	boost::beast::http::request<boost::beast::http::string_body>         req_;
	boost::beast::flat_buffer                                            buffer_;
	boost::beast::http::response_parser<boost::beast::http::string_body> parser_;
};

template <typename Callback>
class plain_connection : public connection<plain_connection<Callback>, Callback>,
                         public std::enable_shared_from_this<plain_connection<Callback>> {
	using base_t = connection<plain_connection<Callback>, Callback>;

public:
	plain_connection(boost::asio::io_context& ioc, int max_redirect, Callback&& cb)
	    : base_t(ioc, max_redirect, std::move(cb), false)
	    , stream_(boost::asio::make_strand(ioc))
	{
	}

	plain_connection(boost::asio::io_context& ioc, int max_redirect, Callback&& cb,
	                 boost::beast::tcp_stream&& stream)
	    : base_t(ioc, max_redirect, std::move(cb), true)
	    , stream_(std::move(stream))
	{
	}

	boost::beast::tcp_stream& stream()
	{
		return stream_;
	}

	boost::beast::tcp_stream&& release_stream()
	{
		return std::move(stream_);
	}

	void hook_connected()
	{
		spdlog::debug("Connection established");
		base_t::send_request();
	}

private:
	boost::beast::tcp_stream stream_;
};

template <typename Callback>
class ssl_connection : public connection<ssl_connection<Callback>, Callback>,
                       public std::enable_shared_from_this<ssl_connection<Callback>> {
	using base_t = connection<ssl_connection<Callback>, Callback>;

public:
	ssl_connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	               int max_redirect, Callback&& cb)
	    : base_t(ioc, max_redirect, std::move(cb), false)
	    , stream_(boost::asio::make_strand(ioc), ssl_ctx)
	{
	}

	ssl_connection(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	               int max_redirect, Callback&& cb, boost::beast::tcp_stream&& stream)
	    : base_t(ioc, max_redirect, std::move(cb), true)
	    , stream_(std::move(stream), ssl_ctx)
	{
	}

	boost::beast::ssl_stream<boost::beast::tcp_stream>& stream()
	{
		return stream_;
	}

	boost::beast::ssl_stream<boost::beast::tcp_stream>&& release_stream()
	{
		return std::move(stream_);
	}

	void hook_connected()
	{
		if (skip_handshake_)
			return base_t::send_request();
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
			return;
		}

		boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

		base_t::send_request();
	}

private:
	boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
	bool                                               skip_handshake_ {false};
};

class connection_handler : public std::enable_shared_from_this<connection_handler> {
public:
	connection_handler(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	                   bbget::proxy::config&& proxy_config)
	    : ioc_(ioc)
	    , ssl_ctx_(ssl_ctx)
	    , proxy_config_(std::move(proxy_config))
	{
	}

	bool create(const std::string& url_string, int max_redirect)
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

		if (!url.has_port())
			url.set_port(url.scheme_id() == boost::urls::scheme::https ? 443 : 80);

		std::string host = url.encoded_host();
		std::string port = url.port();

		switch (url.scheme_id()) {
		case boost::urls::scheme::http:
			return (proxy_config_.enabled)
			    ? proxy_connection(proxy_config_.host, proxy_config_.port, url, max_redirect)
			    : direct_connection(host, port, url, max_redirect);
		case boost::urls::scheme::https:
			return (proxy_config_.enabled)
			    ? tunnel_connection(proxy_config_.host, proxy_config_.port, url, max_redirect)
			    : direct_connection(host, port, url, max_redirect);
		default:
			spdlog::error("Unsupported URL scheme id {}", url.scheme());
			return false;
		}

		return true;
	}

private:
	boost::beast::http::request<boost::beast::http::string_body>
	create_request(boost::beast::http::verb verb, const std::string& target,
	               const std::string& host)
	{
		boost::beast::http::request<boost::beast::http::string_body> req(verb, target, 11);
		req.set(boost::beast::http::field::host, host);
		req.set(boost::beast::http::field::user_agent, "BBGet");
		return req;
	}

	auto create_callback(const boost::urls::url& url)
	{
		return [this, self = shared_from_this(), url = url](auto&& conn, auto&& header) {
			spdlog::debug("{}", header);

			if (redirect(header)) {
				auto location = header.find(boost::beast::http::field::location);

				if (location == header.end()) {
					spdlog::error("Malformed HTTP Response (missing Location)");
					return true;
				}

				if (conn->max_redirect() == 0) {
					spdlog::error("Redirect count exceeded");
					return true;
				}

				std::string loc(header.at(boost::beast::http::field::location));

				spdlog::debug("Redirecting to {}", loc);
				self->create(loc, conn->max_redirect() - 1);

				return true;
			}

			if (conn->get_request().method() == boost::beast::http::verb::connect
			    && header.result_int() == 200) {
				using T = std::remove_cv_t<std::remove_reference_t<decltype(conn->stream())>>;
				if constexpr (std::is_same_v<T, boost::beast::tcp_stream>)
					return send_over_tunnel(conn->release_stream(), url, conn->max_redirect());
			}

			return false;
		};
	}

	template <class Body, class Fields>
	static bool redirect(const boost::beast::http::message<false, Body, Fields>& rsp)
	{
		switch (rsp.result()) {
		case boost::beast::http::status::multiple_choices:
		case boost::beast::http::status::moved_permanently:
		case boost::beast::http::status::found:
		case boost::beast::http::status::see_other:
		case boost::beast::http::status::not_modified:
		case boost::beast::http::status::temporary_redirect:
		case boost::beast::http::status::permanent_redirect:
			return true;
		}
		return false;
	}

	bool direct_connection(const std::string& host, const std::string& port,
	                       const boost::urls::url& url, int max_redirect)
	{
		spdlog::debug("{}", __func__);
		auto req
		    = create_request(boost::beast::http::verb::get, url.encoded_path(), url.encoded_host());

		auto done_cb     = create_callback(url);
		using callback_t = std::decay_t<decltype(done_cb)>;

		if (url.scheme_id() == boost::urls::scheme::https) {
			auto conn = std::make_shared<ssl_connection<callback_t>>(ioc_, ssl_ctx_, max_redirect,
			                                                         std::move(done_cb));
			conn->start(host, port, std::move(req));
		} else if (url.scheme_id() == boost::urls::scheme::http) {
			auto conn = std::make_shared<plain_connection<callback_t>>(ioc_, max_redirect,
			                                                           std::move(done_cb));
			conn->start(host, port, std::move(req));
		} else
			return false;

		return true;
	}

	bool proxy_connection(const std::string& host, const std::string& port,
	                      const boost::urls::url& url, int max_redirect)
	{
		spdlog::debug("{}", __func__);
		auto req = create_request(boost::beast::http::verb::get, url.string(), url.encoded_host());
		if (!proxy_config_.auth.empty())
			req.set(boost::beast::http::field::proxy_authorization, "Basic: " + proxy_config_.auth);

		auto done_cb     = create_callback(url);
		using callback_t = std::decay_t<decltype(done_cb)>;

		if (proxy_config_.ssl) {
			auto conn = std::make_shared<ssl_connection<callback_t>>(ioc_, ssl_ctx_, max_redirect,
			                                                         std::move(done_cb));
			conn->start(host, port, std::move(req));
		} else {
			auto conn = std::make_shared<plain_connection<callback_t>>(ioc_, max_redirect,
			                                                           std::move(done_cb));
			conn->start(host, port, std::move(req));
		}

		return true;
	}

	bool tunnel_connection(const std::string& host, const std::string& port,
	                       const boost::urls::url& url, int max_redirect)
	{
		spdlog::debug("{}", __func__);
		if (proxy_config_.ssl) {
			spdlog::error("Tunneling through TLS connection is not supported");
			return false;
		}

		auto connect     = create_request(boost::beast::http::verb::connect,
                                      url.encoded_host_and_port(), url.encoded_host_and_port());
		auto done_cb     = create_callback(url);
		using callback_t = std::decay_t<decltype(done_cb)>;

		if (!proxy_config_.auth.empty())
			connect.set(boost::beast::http::field::proxy_authorization,
			            "Basic: " + proxy_config_.auth);
		auto conn = std::make_shared<plain_connection<callback_t>>(ioc_, max_redirect,
		                                                           std::move(done_cb));
		conn->start(host, port, std::move(connect));

		return true;
	}

	bool send_over_tunnel(boost::beast::tcp_stream&& stream, const boost::urls::url& url,
	                      int max_redirect)
	{
		spdlog::debug("{}", __func__);
		auto done_cb     = create_callback(url);
		using callback_t = std::decay_t<decltype(done_cb)>;
		auto req
		    = create_request(boost::beast::http::verb::get, url.encoded_path(), url.encoded_host());

		if (url.scheme_id() == boost::urls::scheme::https) {
			auto new_conn = std::make_shared<ssl_connection<callback_t>>(
			    ioc_, ssl_ctx_, max_redirect, std::move(done_cb), std::move(stream));
			new_conn->start(url.encoded_host(), url.port(), std::move(req));
		} else if (url.scheme_id() == boost::urls::scheme::http) {
			auto new_conn = std::make_shared<plain_connection<callback_t>>(
			    ioc_, max_redirect, std::move(done_cb), std::move(stream));
			new_conn->start(url.encoded_host(), url.port(), std::move(req));
		} else
			return false;

		return true;
	}

private:
	boost::asio::io_context&   ioc_;
	boost::asio::ssl::context& ssl_ctx_;
	bbget::proxy::config       proxy_config_;
};

} // namespace outbound
} // namespace http
} // namespace bbget

#endif // HTTP_OUTBOUND_CONNECTION_HPP
