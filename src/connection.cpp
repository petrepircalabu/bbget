#include "connection.hpp"
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/url/url.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>

namespace bbget {
namespace http {
namespace outbound {

class connection {
public:
	connection(boost::asio::io_context& ioc, boost::urls::url&& url)
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

		auto const results = resolver.resolve(host, port);

		stream.connect(results, ec);
		if (ec) {
			spdlog::error("failed to connect to {}:{}, error {}", host, port, ec.what());
			return;
		}

		boost::beast::http::request<boost::beast::http::string_body> req(
		    boost::beast::http::verb::get, url_.c_str(), 11);

		req.set(boost::beast::http::field::host, host);
		req.set(boost::beast::http::field::user_agent, "gogu");

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

	auto conn = std::make_shared<connection>(ioc, std::move(url));
	conn->run();
}

} // namespace outbound
} // namespace http
} // namespace bbget
