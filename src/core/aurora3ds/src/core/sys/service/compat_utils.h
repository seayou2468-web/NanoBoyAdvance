// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include "../../../../include/apple_crypto.hpp"
#include "common/common_types.h"
#include "../../../../include/crypto/platform_random.hpp"

namespace Service::Compat {

inline void ReplaceAll(std::string& text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }

    std::size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.length(), to);
        position += to.length();
    }
}

inline u8 CalculateCrc8(std::span<const u8> data) {
    constexpr u8 polynomial = 0x07;
    u8 remainder = 0;

    for (const u8 byte : data) {
        remainder ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const bool carry = (remainder & 0x80) != 0;
            remainder <<= 1;
            if (carry) {
                remainder ^= polynomial;
            }
        }
    }

    return remainder;
}


inline void GenerateRandomBytes(void* out, std::size_t size) {
    if (!Crypto::GenerateSecureRandomBytes(static_cast<u8*>(out), size)) {
        std::memset(out, 0, size);
    }
}

inline u32 GenerateRandomWord32(u32 min, u32 max) {
    u32 value{};
    GenerateRandomBytes(&value, sizeof(value));
    if (max <= min) {
        return min;
    }
    return min + (value % ((max - min) + 1));
}

inline u8 GenerateRandomByte() {
    u8 value{};
    GenerateRandomBytes(&value, sizeof(value));
    return value;
}

inline std::array<u8, 32> CalculateSha256(std::span<const u8> data) {
    std::array<u8, 32> digest{};
    AppleCrypto::sha256(data.data(), data.size(), digest.data());
    return digest;
}

inline u32 CalculateCrc32(const void* data, std::size_t size) {
    constexpr u32 polynomial = 0xEDB88320U;
    u32 crc = 0xFFFFFFFFU;
    const auto* bytes = static_cast<const u8*>(data);

    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<u32>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const bool carry = (crc & 1U) != 0;
            crc >>= 1;
            if (carry) {
                crc ^= polynomial;
            }
        }
    }

    return ~crc;
}

} // namespace Service::Compat
