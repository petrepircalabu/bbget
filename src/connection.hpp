#ifndef HTTP_OUTBOUND_CONNECTION_HPP
#define HTTP_OUTBOUND_CONNECTION_HPP

#include <boost/asio/io_context.hpp>
#include <string>

namespace bbget {
namespace http {
namespace outbound {

void create_connection(boost::asio::io_context& ioc, const std::string& url_string);

} // namespace outbound
} // namespace http
} // namespace bbget

#endif // HTTP_OUTBOUND_CONNECTION_HPP