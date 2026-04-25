// Copyright 2026 Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "common/common_types.h"

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <Security/SecRandom.h>
#else
#include <cryptopp/aes.h>
#include <cryptopp/cmac.h>
#include <cryptopp/hmac.h>
#include <cryptopp/md5.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#endif

namespace Common::CryptoUtil {

inline bool FillRandomBytes(u8* data, size_t size) {
#if defined(__APPLE__)
    return SecRandomCopyBytes(kSecRandomDefault, size, data) == errSecSuccess;
#else
    CryptoPP::AutoSeededRandomPool rng;
    rng.GenerateBlock(data, size);
    return true;
#endif
}

inline u32 RandomU32(u32 min, u32 max) {
    if (min >= max) {
        return min;
    }

    const u64 range = static_cast<u64>(max) - static_cast<u64>(min) + 1;

#if defined(__APPLE__)
    u32 value = 0;
    if (range == (static_cast<u64>(std::numeric_limits<u32>::max()) + 1ULL)) {
        FillRandomBytes(reinterpret_cast<u8*>(&value), sizeof(value));
        return value;
    }

    const u32 range_u32 = static_cast<u32>(range);
    const u32 limit = std::numeric_limits<u32>::max() -
                      (std::numeric_limits<u32>::max() % range_u32);
    do {
        FillRandomBytes(reinterpret_cast<u8*>(&value), sizeof(value));
    } while (value >= limit);
    return min + (value % range_u32);
#else
    CryptoPP::AutoSeededRandomPool rng;
    return rng.GenerateWord32(min, max);
#endif
}

inline void HmacSha256(std::span<const u8> key, std::span<const u8> data, u8* digest_out) {
#if defined(__APPLE__)
    CCHmac(kCCHmacAlgSHA256, key.data(), key.size(), data.data(), data.size(), digest_out);
#else
    CryptoPP::HMAC<CryptoPP::SHA256> hmac(key.data(), key.size());
    hmac.CalculateDigest(digest_out, data.data(), data.size());
#endif
}

inline void Md5Digest(const u8* data, size_t size, u8* digest_out) {
#if defined(__APPLE__)
    CC_MD5(data, static_cast<CC_LONG>(size), digest_out);
#else
    CryptoPP::Weak::MD5().CalculateDigest(digest_out, data, size);
#endif
}

inline void Sha1Digest(const u8* data, size_t size, u8* digest_out) {
#if defined(__APPLE__)
    CC_SHA1(data, static_cast<CC_LONG>(size), digest_out);
#else
    CryptoPP::SHA1().CalculateDigest(digest_out, data, size);
#endif
}

inline void Sha256Digest(const u8* data, size_t size, u8* digest_out) {
#if defined(__APPLE__)
    CC_SHA256(data, static_cast<CC_LONG>(size), digest_out);
#else
    CryptoPP::SHA256().CalculateDigest(digest_out, data, size);
#endif
}

inline void AesCmac(std::span<const u8> key, std::span<const u8> data, u8* digest_out) {
#if defined(__APPLE__)
    constexpr size_t block_size = 16;
    std::array<u8, block_size> zero_block{};
    std::array<u8, block_size> l{};
    size_t moved = 0;
    CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(), key.size(), nullptr,
            zero_block.data(), zero_block.size(), l.data(), l.size(), &moved);

    const auto left_shift = [](std::array<u8, block_size>& block) {
        u8 carry = 0;
        for (int i = static_cast<int>(block_size) - 1; i >= 0; --i) {
            const u8 next_carry = static_cast<u8>((block[i] >> 7) & 1);
            block[i] = static_cast<u8>((block[i] << 1) | carry);
            carry = next_carry;
        }
        return carry;
    };

    auto k1 = l;
    const u8 msb_l = static_cast<u8>((k1[0] >> 7) & 1);
    left_shift(k1);
    if (msb_l != 0) {
        k1[block_size - 1] ^= 0x87;
    }

    auto k2 = k1;
    const u8 msb_k1 = static_cast<u8>((k2[0] >> 7) & 1);
    left_shift(k2);
    if (msb_k1 != 0) {
        k2[block_size - 1] ^= 0x87;
    }

    size_t n = (data.size() + block_size - 1) / block_size;
    if (n == 0) {
        n = 1;
    }
    const bool last_block_complete = (data.size() != 0) && ((data.size() % block_size) == 0);

    std::array<u8, block_size> x{};
    std::array<u8, block_size> y{};
    std::array<u8, block_size> m_last{};

    const size_t last_block_offset = (n - 1) * block_size;
    if (last_block_complete) {
        std::copy_n(data.data() + last_block_offset, block_size, m_last.data());
        for (size_t i = 0; i < block_size; ++i) {
            m_last[i] ^= k1[i];
        }
    } else {
        const size_t remainder = data.size() - std::min(last_block_offset, data.size());
        if (remainder > 0) {
            std::copy_n(data.data() + last_block_offset, remainder, m_last.data());
        }
        m_last[remainder] = 0x80;
        for (size_t i = 0; i < block_size; ++i) {
            m_last[i] ^= k2[i];
        }
    }

    for (size_t i = 0; i + 1 < n; ++i) {
        for (size_t j = 0; j < block_size; ++j) {
            y[j] = x[j] ^ data[i * block_size + j];
        }
        CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(), key.size(), nullptr,
                y.data(), y.size(), x.data(), x.size(), &moved);
    }

    for (size_t j = 0; j < block_size; ++j) {
        y[j] = x[j] ^ m_last[j];
    }
    CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(), key.size(), nullptr,
            y.data(), y.size(), digest_out, block_size, &moved);
#else
    CryptoPP::CMAC<CryptoPP::AES> cmac(key.data(), key.size());
    cmac.Update(data.data(), data.size());
    cmac.Final(digest_out);
#endif
}

} // namespace Common::CryptoUtil
