#pragma once

#include <algorithm>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace fmt {

using string_view = std::string_view;
using format_args = std::format_args;

template <typename... Args>
using format_string = std::format_string<Args...>;

template <typename... Args>
[[nodiscard]] inline std::string format(format_string<Args...> fmt_str, Args&&... args) {
    return std::format(fmt_str, std::forward<Args>(args)...);
}

[[nodiscard]] inline std::string vformat(string_view fmt_str, format_args args) {
    return std::vformat(fmt_str, args);
}

template <typename... Args>
inline auto make_format_args(Args&... args) {
    return std::make_format_args(args...);
}

template <typename OutputIt, typename... Args>
inline OutputIt format_to(OutputIt out, format_string<Args...> fmt_str, Args&&... args) {
    return std::format_to(out, fmt_str, std::forward<Args>(args)...);
}

template <typename T>
[[nodiscard]] inline const void* ptr(T* p) {
    return static_cast<const void*>(p);
}

template <typename T, typename Char = char>
using formatter = std::formatter<T, Char>;

} // namespace fmt
