#ifndef HTTP_DOWNLOADER_HPP
#define HTTP_DOWNLOADER_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url/url.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>

namespace bbget {
namespace http {

template <typename ConnectionOps> class downloader {
public:
	downloader(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	           const std::string& url, ConnectionOps&& ops)
	    : ioc_(ioc)
	    , ssl_ctx_(ssl_ctx)
	    , url_(url)
	    , ops_(std::forward<ConnectionOps>(ops))
	{
	}

	void operator()()
	{
		auto max_redirect = 3;

		do {
			spdlog::info("Downloading {}", url_);

			auto res = boost::urls::parse_uri(url_);
			if (res.has_error())
				boost::system::throw_exception_from_error(res.error());

			boost::urls::url url = res.value();

			if (!url.has_scheme()) {
				if (url.has_port() && (url.port_number() == 443) || (url.port_number() == 8443))
					url.set_scheme(boost::urls::scheme::https);
				else
					url.set_scheme(boost::urls::scheme::http);
			}

			std::string host = url.host();
			bool        ssl  = url.scheme_id() == boost::urls::scheme::https;
			std::string port = (url.has_port()) ? std::string(url.port()) : ssl ? "443" : "80";

			auto stream = ops_.create(ioc_, ssl_ctx_, host, port, ssl);

			boost::beast::http::request<boost::beast::http::string_body> req(
			    boost::beast::http::verb::get, url_.c_str(), 11);

			req.set(boost::beast::http::field::host, host);
			req.set(boost::beast::http::field::user_agent, "bbget");

			boost::beast::http::write(stream, req);

			boost::beast::flat_buffer                                           buffer;
			boost::beast::http::parser<false, boost::beast::http::dynamic_body> parser;
			// Allow for an unlimited body size
			parser.body_limit((std::numeric_limits<std::uint64_t>::max)());

			boost::beast::http::read_header(stream, buffer, parser);

			boost::beast::http::response<boost::beast::http::dynamic_body> rsp = parser.get();

			switch (rsp.result()) {
			case boost::beast::http::status::multiple_choices:
			case boost::beast::http::status::moved_permanently:
			case boost::beast::http::status::found:
			case boost::beast::http::status::see_other:
			case boost::beast::http::status::not_modified:
			case boost::beast::http::status::temporary_redirect:
			case boost::beast::http::status::permanent_redirect: {
				auto location = rsp.at(boost::beast::http::field::location);
				spdlog::debug("{}", rsp);
				spdlog::info("Redirecting to {}", location);
				url_ = std::string(location);
				continue;
			}
			default:
				break;
			}

			boost::beast::http::read(stream, buffer, parser);

			rsp = parser.release();
			spdlog::info("{}", rsp);

			boost::system::error_code ec {};
			ops_.shutdown(stream, ec);
			break;

		} while (1);
	}

private:
	boost::asio::io_context&   ioc_;
	boost::asio::ssl::context& ssl_ctx_;
	std::string                url_;
	ConnectionOps              ops_;
};

} // namespace http
} // namespace bbget

#endif // HTTP_DOWNLOADER_HPP
