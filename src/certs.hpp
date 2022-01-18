#ifndef CERTS_HPP
#define CERTS_HPP

#include <boost/asio/ssl/context.hpp>

namespace bbget {
namespace certs {

#ifdef WIN32
void add_windows_root_certs(boost::asio::ssl::context& ctx);
#endif

} // namespace certs
} // namespace bbget

#endif // CERTS_HPP