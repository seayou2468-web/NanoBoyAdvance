// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecRandom.h>
#else
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#endif
#include "common/common_types.h"

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
#if defined(__APPLE__)
    SecRandomCopyBytes(kSecRandomDefault, size, static_cast<u8*>(out));
#else
    CryptoPP::AutoSeededRandomPool rng;
    rng.GenerateBlock(static_cast<CryptoPP::byte*>(out), size);
#endif
}

inline u32 GenerateRandomWord32(u32 min, u32 max) {
#if defined(__APPLE__)
    u32 value{};
    GenerateRandomBytes(&value, sizeof(value));
    if (max <= min) {
        return min;
    }
    return min + (value % ((max - min) + 1));
#else
    CryptoPP::AutoSeededRandomPool rng;
    return rng.GenerateWord32(min, max);
#endif
}

inline u8 GenerateRandomByte() {
#if defined(__APPLE__)
    u8 value{};
    GenerateRandomBytes(&value, sizeof(value));
    return value;
#else
    CryptoPP::AutoSeededRandomPool rng;
    return rng.GenerateByte();
#endif
}

inline std::array<u8, 32> CalculateSha256(std::span<const u8> data) {
    std::array<u8, 32> digest{};
#if defined(__APPLE__)
    CC_SHA256(data.data(), static_cast<CC_LONG>(data.size()), digest.data());
#else
    CryptoPP::SHA256().CalculateDigest(digest.data(), data.data(), data.size());
#endif
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
