#include "proxy.hpp"

#include <boost/url/url.hpp>
#include <boost/url/authority_view.hpp>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace bbget {
namespace proxy {

std::tuple<std::string, std::string, bool> decode(const std::string& url)
{
	if (std::string::npos != url.find("http")) {
		auto res = boost::urls::parse_uri(url);
		if (res.has_error())
			boost::system::throw_exception_from_error(res.error());
		auto url = res.value();

		return std::make_tuple(url.encoded_host(), url.port(),
		                       url.scheme_id() == boost::urls::scheme::https);
	}

	auto authority = boost::urls::parse_authority(url);
	if (authority.has_error())
		boost::system::throw_exception_from_error(authority.error());
	auto value = authority.value();
	return std::make_tuple(value.encoded_host(), value.port(),
	                       value.port_number() == 443 || value.port_number() == 8443);
}

} // namespace proxy
} // namespace bbget
