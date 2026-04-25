// Copyright 2026 Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "common/common_types.h"

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#endif

namespace Common::CryptoUtil {

#if defined(__APPLE__)

inline bool Aes128EcbEncryptBlock(std::span<const u8, 16> key, std::span<const u8, 16> input,
                                  std::span<u8, 16> output) {
    size_t moved = 0;
    return CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(), key.size(), nullptr,
                   input.data(), input.size(), output.data(), output.size(), &moved) ==
               kCCSuccess &&
           moved == 16;
}

inline void XorBlock(std::array<u8, 16>& dst, const std::array<u8, 16>& src) {
    for (size_t i = 0; i < 16; ++i) {
        dst[i] ^= src[i];
    }
}

inline std::array<u8, 16> BuildB0(std::span<const u8> nonce, size_t msg_len, size_t tag_len,
                                  bool align_message_length) {
    std::array<u8, 16> b0{};
    const u8 q = static_cast<u8>(15 - nonce.size());
    const size_t auth_len = align_message_length ? ((msg_len + 15) & ~size_t{15}) : msg_len;

    b0[0] = static_cast<u8>(0x40 | (((tag_len - 2) / 2) << 3) | (q - 1));
    std::copy(nonce.begin(), nonce.end(), b0.begin() + 1);

    size_t v = auth_len;
    for (size_t i = 0; i < q; ++i) {
        b0[15 - i] = static_cast<u8>(v & 0xFF);
        v >>= 8;
    }
    return b0;
}

inline std::array<u8, 16> CbcMacAesCcm(std::span<const u8, 16> key, std::span<const u8> nonce,
                                       std::span<const u8> aad, std::span<const u8> msg,
                                       size_t tag_len, bool align_message_length) {
    std::array<u8, 16> x{};
    auto b0 = BuildB0(nonce, msg.size(), tag_len, align_message_length);
    XorBlock(x, b0);
    Aes128EcbEncryptBlock(key, x, x);

    if (!aad.empty()) {
        std::vector<u8> aad_buf;
        aad_buf.reserve(2 + aad.size() + 15);
        aad_buf.push_back(static_cast<u8>((aad.size() >> 8) & 0xFF));
        aad_buf.push_back(static_cast<u8>(aad.size() & 0xFF));
        aad_buf.insert(aad_buf.end(), aad.begin(), aad.end());
        while ((aad_buf.size() % 16) != 0) {
            aad_buf.push_back(0);
        }
        for (size_t offset = 0; offset < aad_buf.size(); offset += 16) {
            std::array<u8, 16> blk{};
            std::copy_n(aad_buf.data() + offset, 16, blk.data());
            XorBlock(x, blk);
            Aes128EcbEncryptBlock(key, x, x);
        }
    }

    size_t offset = 0;
    while (offset < msg.size()) {
        std::array<u8, 16> blk{};
        const size_t take = std::min<size_t>(16, msg.size() - offset);
        std::copy_n(msg.data() + offset, take, blk.data());
        XorBlock(x, blk);
        Aes128EcbEncryptBlock(key, x, x);
        offset += take;
    }
    return x;
}

inline std::array<u8, 16> CtrBlock(std::span<const u8> nonce, size_t counter) {
    std::array<u8, 16> a{};
    const u8 q = static_cast<u8>(15 - nonce.size());
    a[0] = static_cast<u8>(q - 1);
    std::copy(nonce.begin(), nonce.end(), a.begin() + 1);
    for (size_t i = 0; i < q; ++i) {
        a[15 - i] = static_cast<u8>(counter & 0xFF);
        counter >>= 8;
    }
    return a;
}

inline std::vector<u8> AesCcmEncrypt(std::span<const u8, 16> key, std::span<const u8> nonce,
                                     std::span<const u8> aad, std::span<const u8> plaintext,
                                     size_t tag_len, bool align_message_length) {
    auto mac = CbcMacAesCcm(key, nonce, aad, plaintext, tag_len, align_message_length);

    std::vector<u8> out(plaintext.size() + tag_len);
    size_t offset = 0;
    size_t ctr = 1;
    while (offset < plaintext.size()) {
        auto ai = CtrBlock(nonce, ctr++);
        std::array<u8, 16> si{};
        Aes128EcbEncryptBlock(key, ai, si);
        const size_t take = std::min<size_t>(16, plaintext.size() - offset);
        for (size_t i = 0; i < take; ++i) {
            out[offset + i] = plaintext[offset + i] ^ si[i];
        }
        offset += take;
    }

    auto a0 = CtrBlock(nonce, 0);
    std::array<u8, 16> s0{};
    Aes128EcbEncryptBlock(key, a0, s0);
    for (size_t i = 0; i < tag_len; ++i) {
        out[plaintext.size() + i] = static_cast<u8>(mac[i] ^ s0[i]);
    }
    return out;
}

inline std::vector<u8> AesCcmDecrypt(std::span<const u8, 16> key, std::span<const u8> nonce,
                                     std::span<const u8> aad, std::span<const u8> ciphertext,
                                     std::span<const u8> tag, bool align_message_length) {
    std::vector<u8> plaintext(ciphertext.size());
    size_t offset = 0;
    size_t ctr = 1;
    while (offset < ciphertext.size()) {
        auto ai = CtrBlock(nonce, ctr++);
        std::array<u8, 16> si{};
        Aes128EcbEncryptBlock(key, ai, si);
        const size_t take = std::min<size_t>(16, ciphertext.size() - offset);
        for (size_t i = 0; i < take; ++i) {
            plaintext[offset + i] = ciphertext[offset + i] ^ si[i];
        }
        offset += take;
    }

    auto mac = CbcMacAesCcm(key, nonce, aad, plaintext, tag.size(), align_message_length);
    auto a0 = CtrBlock(nonce, 0);
    std::array<u8, 16> s0{};
    Aes128EcbEncryptBlock(key, a0, s0);

    u8 diff = 0;
    for (size_t i = 0; i < tag.size(); ++i) {
        diff |= static_cast<u8>((mac[i] ^ s0[i]) ^ tag[i]);
    }
    if (diff != 0) {
        return {};
    }
    return plaintext;
}

#endif

} // namespace Common::CryptoUtil
