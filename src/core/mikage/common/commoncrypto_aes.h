// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#if __has_include(<CommonCrypto/CommonCryptor.h>)
#include <CommonCrypto/CommonCryptor.h>
#define MIKAGE_HAS_COMMONCRYPTO 1
#else
#define MIKAGE_HAS_COMMONCRYPTO 0
#endif

namespace Common::Crypto {

constexpr std::size_t AESBlockSize = 16;
using AESBlock = std::array<std::uint8_t, AESBlockSize>;

inline void IncrementCtr(AESBlock& ctr, std::uint64_t blocks) {
    while (blocks--) {
        for (std::size_t i = AESBlockSize; i-- > 0;) {
            if (++ctr[i] != 0) {
                break;
            }
        }
    }
}

inline bool AESCBCDecrypt(std::span<const std::uint8_t> in, std::span<std::uint8_t> out,
                          std::span<const std::uint8_t> key, const AESBlock& iv) {
#if MIKAGE_HAS_COMMONCRYPTO
    if (in.size() != out.size() || (in.size() % AESBlockSize) != 0) {
        return false;
    }

    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus create_status = CCCryptorCreateWithMode(
        kCCDecrypt, kCCModeCBC, kCCAlgorithmAES, ccNoPadding, iv.data(), key.data(), key.size(),
        nullptr, 0, 0, 0, &cryptor);
    if (create_status != kCCSuccess || cryptor == nullptr) {
        return false;
    }

    size_t out_len = 0;
    const CCCryptorStatus update_status =
        CCCryptorUpdate(cryptor, in.data(), in.size(), out.data(), out.size(), &out_len);
    CCCryptorRelease(cryptor);
    return update_status == kCCSuccess && out_len == out.size();
#else
    (void)in;
    (void)out;
    (void)key;
    (void)iv;
    return false;
#endif
}

inline bool AESCBCEncrypt(std::span<const std::uint8_t> in, std::span<std::uint8_t> out,
                          std::span<const std::uint8_t> key, const AESBlock& iv) {
#if MIKAGE_HAS_COMMONCRYPTO
    if (in.size() != out.size() || (in.size() % AESBlockSize) != 0) {
        return false;
    }

    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus create_status = CCCryptorCreateWithMode(
        kCCEncrypt, kCCModeCBC, kCCAlgorithmAES, ccNoPadding, iv.data(), key.data(), key.size(),
        nullptr, 0, 0, 0, &cryptor);
    if (create_status != kCCSuccess || cryptor == nullptr) {
        return false;
    }

    size_t out_len = 0;
    const CCCryptorStatus update_status =
        CCCryptorUpdate(cryptor, in.data(), in.size(), out.data(), out.size(), &out_len);
    CCCryptorRelease(cryptor);
    return update_status == kCCSuccess && out_len == out.size();
#else
    (void)in;
    (void)out;
    (void)key;
    (void)iv;
    return false;
#endif
}

inline bool AESCTRTransform(std::span<const std::uint8_t> in, std::span<std::uint8_t> out,
                            std::span<const std::uint8_t> key, AESBlock ctr,
                            std::uint64_t byte_offset = 0) {
#if MIKAGE_HAS_COMMONCRYPTO
    if (in.size() != out.size()) {
        return false;
    }

    IncrementCtr(ctr, byte_offset / AESBlockSize);

    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus create_status = CCCryptorCreateWithMode(
        kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, ccNoPadding, ctr.data(), key.data(), key.size(),
        nullptr, 0, 0, kCCModeOptionCTR_BE, &cryptor);
    if (create_status != kCCSuccess || cryptor == nullptr) {
        return false;
    }

    const std::size_t offset_in_block = byte_offset % AESBlockSize;
    if (offset_in_block != 0) {
        std::array<std::uint8_t, AESBlockSize> zero{};
        std::array<std::uint8_t, AESBlockSize> discard{};
        size_t consumed = 0;
        const CCCryptorStatus skip_status = CCCryptorUpdate(cryptor, zero.data(), offset_in_block,
                                                            discard.data(), discard.size(), &consumed);
        if (skip_status != kCCSuccess || consumed != offset_in_block) {
            CCCryptorRelease(cryptor);
            return false;
        }
    }

    size_t out_len = 0;
    const CCCryptorStatus update_status =
        CCCryptorUpdate(cryptor, in.data(), in.size(), out.data(), out.size(), &out_len);
    CCCryptorRelease(cryptor);
    return update_status == kCCSuccess && out_len == out.size();
#else
    (void)in;
    (void)out;
    (void)key;
    (void)ctr;
    (void)byte_offset;
    return false;
#endif
}

inline AESBlock XorBlock(const AESBlock& a, const AESBlock& b) {
    AESBlock out{};
    for (std::size_t i = 0; i < AESBlockSize; ++i) {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

inline AESBlock LeftShiftOne(const AESBlock& in) {
    AESBlock out{};
    std::uint8_t carry = 0;
    for (std::size_t i = AESBlockSize; i-- > 0;) {
        const std::uint8_t next_carry = (in[i] & 0x80) != 0 ? 1 : 0;
        out[i] = static_cast<std::uint8_t>((in[i] << 1) | carry);
        carry = next_carry;
    }
    return out;
}

inline bool AESECBEncryptBlock(const AESBlock& in, std::span<const std::uint8_t> key,
                               AESBlock& out) {
#if MIKAGE_HAS_COMMONCRYPTO
    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus create_status =
        CCCryptorCreateWithMode(kCCEncrypt, kCCModeECB, kCCAlgorithmAES, ccNoPadding, nullptr,
                                key.data(), key.size(), nullptr, 0, 0, 0, &cryptor);
    if (create_status != kCCSuccess || cryptor == nullptr) {
        return false;
    }

    size_t out_len = 0;
    const CCCryptorStatus update_status =
        CCCryptorUpdate(cryptor, in.data(), in.size(), out.data(), out.size(), &out_len);
    CCCryptorRelease(cryptor);
    return update_status == kCCSuccess && out_len == out.size();
#else
    (void)in;
    (void)key;
    (void)out;
    return false;
#endif
}

inline bool AESCMAC(std::span<const std::uint8_t> message, std::span<const std::uint8_t> key,
                    AESBlock& mac_out) {
#if MIKAGE_HAS_COMMONCRYPTO
    constexpr AESBlock rb = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};
    AESBlock zero{};
    AESBlock l{};
    if (!AESECBEncryptBlock(zero, key, l)) {
        return false;
    }

    AESBlock k1 = LeftShiftOne(l);
    if ((l[0] & 0x80) != 0) {
        k1 = XorBlock(k1, rb);
    }
    AESBlock k2 = LeftShiftOne(k1);
    if ((k1[0] & 0x80) != 0) {
        k2 = XorBlock(k2, rb);
    }

    const bool complete_last_block =
        !message.empty() && (message.size() % AESBlockSize == 0);
    const std::size_t block_count =
        message.empty() ? 1 : (message.size() + AESBlockSize - 1) / AESBlockSize;

    AESBlock x{};
    for (std::size_t i = 0; i + 1 < block_count; ++i) {
        AESBlock block{};
        std::memcpy(block.data(), message.data() + i * AESBlockSize, AESBlockSize);
        const AESBlock y = XorBlock(x, block);
        if (!AESECBEncryptBlock(y, key, x)) {
            return false;
        }
    }

    AESBlock last{};
    const std::size_t last_offset = (block_count - 1) * AESBlockSize;
    const std::size_t remaining = message.size() - last_offset;
    if (complete_last_block) {
        std::memcpy(last.data(), message.data() + last_offset, AESBlockSize);
        last = XorBlock(last, k1);
    } else {
        if (remaining > 0) {
            std::memcpy(last.data(), message.data() + last_offset, remaining);
        }
        last[remaining] = 0x80;
        last = XorBlock(last, k2);
    }

    const AESBlock y = XorBlock(x, last);
    return AESECBEncryptBlock(y, key, mac_out);
#else
    (void)message;
    (void)key;
    (void)mac_out;
    return false;
#endif
}

} // namespace Common::Crypto
