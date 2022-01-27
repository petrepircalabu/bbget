#include "connection.hpp"
#include "stream.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>

namespace {

bbget::http::outbound::stream create_plain_connection(boost::asio::io_context& ioc,
                                                      const std::string&       host,
                                                      const std::string&       port)
{
	boost::asio::ip::tcp::resolver resolver(ioc);
	boost::beast::tcp_stream       stream(boost::asio::make_strand(ioc));

	auto const results = resolver.resolve(host, port);
	stream.connect(results);

	return stream;
}

bbget::http::outbound::stream create_ssl_connection(boost::asio::io_context&   ioc,
                                                    boost::asio::ssl::context& ssl_ctx,
                                                    const std::string&         host,
                                                    const std::string&         port)
{
	boost::asio::ip::tcp::resolver resolver(ioc);
	boost::beast::tcp_stream       stream(boost::asio::make_strand(ioc));

	auto const results = resolver.resolve(host, port);
	stream.connect(results);

	boost::beast::ssl_stream<boost::beast::tcp_stream> ssl_stream(std::move(stream), ssl_ctx);

	ssl_stream.handshake(boost::asio::ssl::stream_base::handshake_type::client);

	return ssl_stream;
}

}

namespace bbget {
namespace http {
namespace outbound {

stream connection_ops::create(boost::asio::io_context& ioc, boost::asio::ssl::context& ssl_ctx,
                              const std::string& host, const std::string& port, bool ssl)
{
	return (ssl) ? create_ssl_connection(ioc, ssl_ctx, host, port)
	             : create_plain_connection(ioc, host, port);
}

void connection_ops::shutdown(connection_ops::stream_type& stream)
{
	stream.shutdown();
}

void connection_ops::shutdown(connection_ops::stream_type& stream, boost::system::error_code& ec)
{
	stream.shutdown(ec);
}

} // namespace outbound
} // namespace http
} // namespace bbget
