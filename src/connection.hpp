#ifndef HTTP_OUTBOUND_CONNECTION_HPP
#define HTTP_OUTBOUND_CONNECTION_HPP

#include "stream.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <variant>

namespace bbget {
namespace http {
namespace outbound {

struct connection_ops {
	using stream_type = stream;

	stream_type create(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
	                   const std::string& host, const std::string& port, bool ssl);

	void shutdown(stream_type& stream);

	void shutdown(connection_ops::stream_type& stream, boost::system::error_code& ec);
};

} // namespace outbound
} // namespace http
} // namespace bbget

#endif // HTTP_OUTBOUND_CONNECTION_HPP
