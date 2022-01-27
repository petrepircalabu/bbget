#ifndef HTTP_STREAM_HPP
#define HTTP_STREAM_HPP

#include "overload.hpp"
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <variant>

namespace bbget {
namespace http {
namespace outbound {

class stream {
public:
	using stream_type = std::variant<boost::beast::tcp_stream,
	                                 boost::beast::ssl_stream<boost::beast::tcp_stream>>;

	stream(boost::beast::tcp_stream&& stream)
	    : impl_(std::forward<boost::beast::tcp_stream>(stream))
	{
	}

	stream(boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream)
	    : impl_(std::forward<boost::beast::ssl_stream<boost::beast::tcp_stream>>(stream))
	{
	}

	template <class ConstBufferSequence> std::size_t write_some(ConstBufferSequence const& buffers)
	{
		return std::visit([&](auto&& stream) { return stream.write_some(buffers); }, impl_);
	}

	template <class ConstBufferSequence>
	std::size_t write_some(ConstBufferSequence const& buffers, boost::beast::error_code& ec)
	{
		return std::visit([&](auto&& stream) { return stream.write_some(buffers, ec); }, impl_);
	}

	template <class MutableBufferSequence>
	std::size_t read_some(MutableBufferSequence const& buffers)
	{
		return std::visit([&](auto&& stream) { return stream.read_some(buffers); }, impl_);
	}

	template <class MutableBufferSequence>
	std::size_t read_some(MutableBufferSequence const& buffers, boost::beast::error_code& ec)
	{
		return std::visit([&](auto&& stream) { return stream.read_some(buffers, ec); }, impl_);
	}

	void shutdown()
	{
		std::visit(bbget::utils::overload {
		               [](boost::beast::tcp_stream& stream) {
			               stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
		               },
		               [](boost::beast::ssl_stream<boost::beast::tcp_stream>& stream) {
			               stream.shutdown();
			               boost::beast::get_lowest_layer(stream).socket().close();
		               }},
		           impl_);
	}

	void shutdown(boost::system::error_code& ec)
	{
		std::visit(bbget::utils::overload {
		               [&](boost::beast::tcp_stream& stream) {
			               stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
			                                        ec);
		               },
		               [&](boost::beast::ssl_stream<boost::beast::tcp_stream>& stream) {
			               stream.shutdown(ec);
			               boost::beast::get_lowest_layer(stream).socket().close();
		               }},
		           impl_);
	}

private:
	stream_type impl_;
};

}
} // namespace http
} // namespace bbget

#endif // HTTP_STREAM_HPP
