#pragma once

#include <type_traits>

namespace Common {

template <typename T>
constexpr T AlignUp(T value, T alignment) {
    static_assert(std::is_integral_v<T>);
    return (value + alignment - 1) / alignment * alignment;
}

template <typename T>
constexpr T AlignDown(T value, T alignment) {
    static_assert(std::is_integral_v<T>);
    return value / alignment * alignment;
}

} // namespace Common
