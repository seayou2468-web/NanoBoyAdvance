// iOS Compatibility Layer for Common ARM Types
// Provides standard integer type definitions
#pragma once

#include <cstdint>
#include <limits>

// Standard integer types (already in C++11 via cstdint)
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

// Standard floating-point types
using f32 = float;
using f64 = double;

// Common ARM-specific constants
constexpr u32 BITS_PER_BYTE = 8;
constexpr u32 MAX_U32 = std::numeric_limits<u32>::max();
constexpr u32 MIN_U32 = std::numeric_limits<u32>::min();
