#include "connection.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url/url.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>

namespace bbget {
namespace http {
namespace outbound {

class plain_connection {
public:
	plain_connection(boost::asio::io_context& ioc, boost::urls::url&& url)
	    : ioc_(ioc)
	    , url_(std::forward<boost::urls::url>(url))
	{
	}

	void run()
	{
		boost::asio::ip::tcp::resolver resolver(ioc_);
		boost::beast::tcp_stream       stream(ioc_);
		boost::system::error_code      ec {};

		std::string host = url_.host();
		std::string port = url_.port();

		boost::beast::http::request<boost::beast::http::string_body> req(
		    boost::beast::http::verb::get, url_.c_str(), 11);

		req.set(boost::beast::http::field::host, host);
		req.set(boost::beast::http::field::user_agent, "gogu");

		auto const results = resolver.resolve(host, port, ec);
		if (ec || results.empty()) {
			spdlog::error("failed to resolve to {}:{}, error {}", host, port, ec.what());
			return;
		}

		stream.connect(results, ec);
		if (ec) {
			spdlog::error("failed to connect to {}:{}, error {}", host, port, ec.what());
			return;
		}

		boost::beast::http::write(stream, req);

		boost::beast::flat_buffer                                      buffer;
		boost::beast::http::response<boost::beast::http::dynamic_body> res;

		boost::beast::http::read(stream, buffer, res, ec);
		if (ec) {
			spdlog::error("failed toread server response, error {}", ec.what());
			return;
		}

		spdlog::info("{}", res);

		stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	}

private:
	boost::asio::io_context& ioc_;
	boost::urls::url         url_;
};

class ssl_connection {
public:
	ssl_connection(boost::asio::io_context& ioc, boost::urls::url&& url)
	    : ioc_(ioc)
	    , ssl_ctx_(boost::asio::ssl::context::tlsv13_client)
	    , url_(std::forward<boost::urls::url>(url))
	{
	}

	void run()
	{
		std::string host = url_.host();
		std::string port = url_.port();

		boost::beast::http::request<boost::beast::http::string_body> req(
		    boost::beast::http::verb::get, url_.c_str(), 11);
		req.set(boost::beast::http::field::host, host);
		req.set(boost::beast::http::field::user_agent, "gogu");

		boost::system::error_code ec {};

		boost::beast::tcp_stream stream {ioc_};

		auto const results = resolve(host, port, ec);
		if (ec || results.empty()) {
			spdlog::error("failed to resolve to {}:{}, error {}", host, port, ec.what());
			return;
		}

		stream.connect(results, ec);
		if (ec) {
			spdlog::error("failed to connect to {}:{}, error {}", host, port, ec.what());
			return;
		}

		ssl_ctx_.set_verify_mode(boost::asio::ssl::context::verify_peer
		                         | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
		ssl_ctx_.set_default_verify_paths(ec);
		if (ec) {
			spdlog::error("Cannot set the SSL context default paths, error {}", host, port, ec.what());
			return;
		}

		boost::beast::ssl_stream<boost::beast::tcp_stream> ssl_stream(std::move(stream), ssl_ctx_);

		ssl_stream.handshake(boost::asio::ssl::stream_base::handshake_type::client, ec);
		if (ec) {
			spdlog::error("Handshake failed, error {}", ec.what());
			return;
		}

		boost::beast::http::write(ssl_stream, req);

		boost::beast::flat_buffer                                     buffer;
		boost::beast::http::response<boost::beast::http::string_body> res;

		boost::beast::http::read(ssl_stream, buffer, res, ec);
		if (ec) {
			spdlog::error("failed toread server response, error {}", ec.what());
			return;
		}

		spdlog::info("{}", res);

		ssl_stream.shutdown(ec);
		boost::beast::get_lowest_layer(ssl_stream).socket().close();
	}

protected:
	inline boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>
	resolve(std::string_view host, std::string_view port, boost::system::error_code& ec)
	{
		boost::asio::ip::tcp::resolver resolver(ioc_);
		return resolver.resolve(host, port, ec);
	}

private:
	boost::asio::io_context&  ioc_;
	boost::asio::ssl::context ssl_ctx_;

	boost::urls::url url_;
};

void create_connection(boost::asio::io_context& ioc, const std::string& url_string)
{
	auto res = boost::urls::parse_uri(url_string);
	if (res.error()) {
		spdlog::error("Failed to parse uri {}, error: {}", url_string, res.error().what());
		return;
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

	if (url.scheme_id() == boost::urls::scheme::https) {
		auto conn = std::make_shared<ssl_connection>(ioc, std::move(url));
		conn->run();
	} else {
		auto conn = std::make_shared<plain_connection>(ioc, std::move(url));
		conn->run();
	}
}

} // namespace outbound
} // namespace http
} // namespace bbget
