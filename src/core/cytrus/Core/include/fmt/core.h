#pragma once

#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace fmt {

using string_view = std::string_view;

struct format_args {};

template <typename... Args>
inline format_args make_format_args(Args&&...) {
  return {};
}

struct format_parse_context {
  constexpr const char* begin() const { return ""; }
};

template <typename OutIt>
class basic_format_context {
 public:
  explicit basic_format_context(OutIt out) : out_(out) {}
  OutIt out() { return out_; }

 private:
  OutIt out_;
};

using format_context = basic_format_context<std::back_insert_iterator<std::string>>;

template <typename... Args>
class format_string {
 public:
  explicit constexpr format_string(const char* s) : str_(s) {}
  constexpr const char* get() const { return str_; }

 private:
  const char* str_;
};

template <typename T>
struct formatter {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const T&, FormatContext& ctx) const {
    return ctx.out();
  }
};

template <>
struct formatter<std::string_view> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(std::string_view value, FormatContext& ctx) const {
    auto out = ctx.out();
    for (char c : value) {
      *out++ = c;
    }
    return out;
  }
};

template <typename... Args>
inline std::string format(format_string<Args...> fmt_str, Args&&...) {
  return std::string(fmt_str.get());
}

inline std::string vformat(string_view fmt_str, format_args) {
  return std::string(fmt_str);
}

template <typename OutIt, typename... Args>
inline OutIt format_to(OutIt out, format_string<Args...> fmt_str, Args&&... args) {
  std::string formatted = format(fmt_str, std::forward<Args>(args)...);
  for (char c : formatted) {
    *out++ = c;
  }
  return out;
}

template <typename T>
inline const void* ptr(const T* p) {
  return static_cast<const void*>(p);
}

template <typename It>
inline std::string join(It begin, It end, string_view sep) {
  std::ostringstream oss;
  bool first = true;
  for (It it = begin; it != end; ++it) {
    if (!first) oss << sep;
    first = false;
    oss << *it;
  }
  return oss.str();
}

template <typename Range>
inline std::string join(const Range& range, string_view sep) {
  return join(std::begin(range), std::end(range), sep);
}

}  // namespace fmt
