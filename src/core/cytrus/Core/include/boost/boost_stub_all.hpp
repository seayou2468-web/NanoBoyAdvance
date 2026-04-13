#pragma once

#include <bitset>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace boost {

namespace algorithm::string {
inline void replace_all(std::string& input, std::string_view from, std::string_view to) {
  if (from.empty()) return;
  std::size_t pos = 0;
  while ((pos = input.find(from, pos)) != std::string::npos) {
    input.replace(pos, from.size(), to);
    pos += to.size();
  }
}
}  // namespace algorithm::string

template <typename T>
using optional = std::optional<T>;

namespace container {
template <typename K, typename V, typename Compare = std::less<K>, typename Alloc = std::allocator<std::pair<const K, V>>>
using flat_map = std::map<K, V, Compare, Alloc>;
template <typename T, typename Compare = std::less<T>, typename Alloc = std::allocator<T>>
using flat_set = std::set<T, Compare, Alloc>;
template <typename T, typename Alloc = std::allocator<T>>
using vector = std::vector<T, Alloc>;
template <typename T, std::size_t N>
using static_vector = std::vector<T>;
template <typename T, std::size_t N, typename Alloc = std::allocator<T>>
using small_vector = std::vector<T, Alloc>;
}  // namespace container

namespace range {
template <typename It>
class iterator_range {
 public:
  iterator_range(It b, It e) : b_(b), e_(e) {}
  It begin() const { return b_; }
  It end() const { return e_; }

 private:
  It b_;
  It e_;
};
}  // namespace range

namespace regex = std;
using regex = std::regex;
using smatch = std::smatch;
using regex_error = std::regex_error;

namespace stacktrace {
class stacktrace {};
inline stacktrace stacktrace_from_exception(const std::exception&) { return {}; }
inline std::string to_string(const stacktrace&) { return {}; }
}  // namespace stacktrace

namespace mpl {
template <bool B>
using bool_ = std::bool_constant<B>;
template <typename Cond, typename T, typename F>
using if_ = std::conditional_t<Cond::value, T, F>;
}  // namespace mpl



namespace icl {
template <typename T>
class interval_set {};
template <typename K, typename V>
class interval_map {};
template <typename T>
class right_open_interval {};
template <typename T>
class discrete_interval {};
}  // namespace icl

namespace iostreams {
class file_descriptor_source {
 public:
  explicit file_descriptor_source(const char*) {}
};
template <typename T>
class stream {
 public:
  explicit stream(T) {}
};
}  // namespace iostreams

namespace locale::conv {
inline std::string utf_to_utf(const std::string& in) { return in; }
}  // namespace locale::conv

namespace hana {
template <typename T>
constexpr T string(T value) {
  return value;
}
}  // namespace hana

namespace asio {
class io_context {};
}  // namespace asio

namespace archive {
class archive_exception {};
class binary_iarchive {
 public:
  template <typename T>
  explicit binary_iarchive(T&) {}
  template <typename T>
  binary_iarchive& operator>>(T&) { return *this; }
  template <typename T>
  binary_iarchive& operator&(T&) { return *this; }
};
class binary_oarchive {
 public:
  template <typename T>
  explicit binary_oarchive(T&) {}
  template <typename T>
  binary_oarchive& operator<<(const T&) { return *this; }
  template <typename T>
  binary_oarchive& operator&(T&) { return *this; }
};
namespace detail {
class basic_iarchive {};
}  // namespace detail
}  // namespace archive

namespace serialization {
class access {};
template <typename T>
T& base_object(T& v) { return v; }

template <typename Archive, typename T>
void split_member(Archive&, T&, unsigned int) {}

template <typename Archive, typename T>
void split_free(Archive&, T&, unsigned int) {}

template <typename T>
T& make_nvp(const char*, T& v) { return v; }

template <typename T>
T& make_binary_object(T& v, std::size_t) { return v; }

template <typename T>
void throw_exception(T&&) {}

class collection_size_type {
 public:
  explicit collection_size_type(std::size_t) {}
};
class item_version_type {
 public:
  explicit item_version_type(unsigned int) {}
};
class tracking_type {
 public:
  explicit tracking_type(bool) {}
};
}  // namespace serialization

}  // namespace boost

#define BOOST_SERIALIZATION_SPLIT_MEMBER() \
  template <class Archive> void serialize(Archive&, const unsigned int)
#define BOOST_SERIALIZATION_SPLIT_FREE(T)
#define BOOST_SERIALIZATION_ASSUME_ABSTRACT(...)
#define BOOST_CLASS_EXPORT_KEY(...)
#define BOOST_CLASS_EXPORT_IMPLEMENT(...)
#define BOOST_CLASS_EXPORT(...)
#define BOOST_WORKAROUND(symbol, test) 0
#define BOOST_HANA_STRING(x) x
