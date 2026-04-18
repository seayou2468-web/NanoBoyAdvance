// iOS-first Compatibility Layer for Endian Byte Swapping
#pragma once

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace Common {

inline uint16_t swap16(uint16_t value) {
#if defined(_MSC_VER)
    return _byteswap_ushort(value);
#else
    return static_cast<uint16_t>((value >> 8) | (value << 8));
#endif
}

inline uint32_t swap32(uint32_t value) {
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#else
    return __builtin_bswap32(value);
#endif
}

inline uint64_t swap64(uint64_t value) {
#if defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    return __builtin_bswap64(value);
#endif
}

template <typename T>
inline void swap(T& a, T& b) {
    std::swap(a, b);
}

} // namespace Common
