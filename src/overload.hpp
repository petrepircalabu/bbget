#ifndef UTILS_OVERLOAD_HPP
#define UTILS_OVERLOAD_HPP

namespace bbget {
namespace utils {

template <typename... Ts> struct overload : Ts... {
	using Ts::operator()...;
};
template <class... Ts> overload(Ts...) -> overload<Ts...>;

} // namespace utils
} // namespace bbget

#endif // UTILS_OVERLOAD_HPP
