#ifndef HTTP_PROXY_HPP
#define HTTP_PROXY_HPP

#include <string>
#include <tuple>

namespace bbget {
namespace proxy {

struct config {
	bool        enabled {false};
	std::string host;
	std::string port;
	bool        ssl {false};
	bool        check_certificate {false};
	std::string auth;
};

std::tuple<std::string, std::string, bool> decode(const std::string& str);

} // namespace proxy
} // namespace bbget

#endif // HTTP_PROXY_HPP