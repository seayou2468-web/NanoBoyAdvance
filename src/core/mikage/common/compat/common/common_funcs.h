// iOS Compatibility Layer for Common Functions
// Utility functions for byte/bit manipulation
#pragma once

#include <cstdint>
#include <bit>  // C++20 for std::rotr, std::rotl

// Bit rotation utilities
inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

// Count leading/trailing zeros
inline int clz(uint32_t x) {
    if (x == 0) return 32;
    return __builtin_clz(x);
}

inline int ctz(uint32_t x) {
    if (x == 0) return 32;
    return __builtin_ctz(x);
}

// Popcount (count set bits)
inline int popcount(uint32_t x) {
    return __builtin_popcount(x);
}
