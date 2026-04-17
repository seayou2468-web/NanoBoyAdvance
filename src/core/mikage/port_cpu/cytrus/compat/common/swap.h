// iOS Compatibility Layer for Endian Byte Swapping
// Provides Common::swap16/32/64 using C++11 and platform-specific code
#pragma once

#include <cstdint>
#include <type_traits>
#include <algorithm>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define HAS_OSBYTEORDER 1
#else
#define HAS_OSBYTEORDER 0
#endif

namespace Common {

// Generic byte swap using platform-specific code if available
inline uint16_t swap16(uint16_t value) {
#if HAS_OSBYTEORDER
    return OSSwapInt16(value);
#elif defined(_MSC_VER)
    return _byteswap_ushort(value);
#else
    // Fallback portable implementation
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
#endif
}

inline uint32_t swap32(uint32_t value) {
#if HAS_OSBYTEORDER
    return OSSwapInt32(value);
#elif defined(_MSC_VER)
    return _byteswap_ulong(value);
#else
    // Fallback portable implementation
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
#endif
}

inline uint64_t swap64(uint64_t value) {
#if HAS_OSBYTEORDER
    return OSSwapInt64(value);
#elif defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    // Fallback portable implementation
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
#endif
}

// Convenience swap function that uses std::swap for non-byte-order scenarios
template <typename T>
inline void swap(T& a, T& b) {
    std::swap(a, b);
}

} // namespace Common
