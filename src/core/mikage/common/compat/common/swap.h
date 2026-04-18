// iOS-first Compatibility Layer for Endian Byte Swapping
#pragma once

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace Common {

inline uint16_t swap16(uint16_t value) {

}

inline uint32_t swap32(uint32_t value) {

}

inline uint64_t swap64(uint64_t value) {

}

template <typename T>
inline void swap(T& a, T& b) {
    std::swap(a, b);
}

} // namespace Common
