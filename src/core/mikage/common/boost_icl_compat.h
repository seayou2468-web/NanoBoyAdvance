// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

// Replacement for external ICL-style interval helpers using C++ standard library
// Provides minimal interval operations needed for rasterizer cache surface handling

#include <utility>
#include <set>
#include <functional>

namespace MikageIcl {

template <typename T>
using right_open_interval = std::pair<T, T>;

template <typename T, typename Compare = std::less<T>, typename Interval = right_open_interval<T>>
using interval_set = std::set<Interval>;

// Get the first (lower bound) value of an interval
template <typename T>
inline auto first(const T& interval) noexcept {
    return interval.first;
}

// Get the value just past the last (upper bound + 1) of an interval
template <typename T>
inline auto last_next(const T& interval) noexcept {
    return interval.second;
}

// Get the length (size) of an interval
template <typename T>
inline auto length(const T& interval) noexcept -> typename T::first_type {
    return interval.second - interval.first;
}

// Standard pair specialization
template<typename T>
inline T first(const std::pair<T, T>& interval) noexcept {
    return interval.first;
}

template<typename T>
inline T last_next(const std::pair<T, T>& interval) noexcept {
    return interval.second;
}

template<typename T>
inline T length(const std::pair<T, T>& interval) noexcept {
    return interval.second - interval.first;
}

} // namespace MikageIcl

namespace boost {
namespace icl = MikageIcl;
}
