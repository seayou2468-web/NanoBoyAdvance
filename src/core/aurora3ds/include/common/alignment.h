// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>

namespace Common {

template <typename T>
[[nodiscard]] constexpr T AlignDown(T value, T alignment) {
    static_assert(std::is_integral_v<T>, "AlignDown requires integral type");
    return alignment == 0 ? value : (value / alignment) * alignment;
}

template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, T alignment) {
    static_assert(std::is_integral_v<T>, "AlignUp requires integral type");
    return alignment == 0 ? value : AlignDown<T>(value + alignment - 1, alignment);
}

}  // namespace Common
