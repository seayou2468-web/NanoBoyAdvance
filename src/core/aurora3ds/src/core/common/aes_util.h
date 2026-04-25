// Copyright 2026 Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#include "common/common_types.h"

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#else
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#endif

namespace Common::AESUtil {

#if defined(__APPLE__)
class AesCbcDecryptor {
public:
    AesCbcDecryptor() = default;
    ~AesCbcDecryptor() {
        if (cryptor != nullptr) {
            CCCryptorRelease(cryptor);
        }
    }

    AesCbcDecryptor(const AesCbcDecryptor&) = delete;
    AesCbcDecryptor& operator=(const AesCbcDecryptor&) = delete;

    AesCbcDecryptor(AesCbcDecryptor&& other) noexcept : cryptor(other.cryptor) {
        other.cryptor = nullptr;
    }
    AesCbcDecryptor& operator=(AesCbcDecryptor&& other) noexcept {
        if (this != &other) {
            if (cryptor != nullptr) {
                CCCryptorRelease(cryptor);
            }
            cryptor = other.cryptor;
            other.cryptor = nullptr;
        }
        return *this;
    }

    void SetKeyWithIV(const u8* key, size_t key_size, const u8* iv_data) {
        if (cryptor != nullptr) {
            CCCryptorRelease(cryptor);
            cryptor = nullptr;
        }
        CCCryptorCreateWithMode(kCCDecrypt, kCCModeCBC, kCCAlgorithmAES, ccNoPadding, iv_data,
                                key, key_size, nullptr, 0, 0, 0, &cryptor);
    }

    void ProcessData(u8* out, const u8* in, size_t size) {
        size_t out_moved = 0;
        CCCryptorUpdate(cryptor, in, size, out, size, &out_moved);
    }

private:
    CCCryptorRef cryptor = nullptr;
};

class AesCbcEncryptor {
public:
    AesCbcEncryptor() = default;
    AesCbcEncryptor(const u8* key_data, size_t key_size, const u8* iv_data) {
        SetKeyWithIV(key_data, key_size, iv_data);
    }

    void SetKeyWithIV(const u8* key_data, size_t key_size, const u8* iv_data) {
        std::memcpy(key.data(), key_data, key_size);
        std::memcpy(iv.data(), iv_data, iv.size());
    }

    void ProcessData(u8* out, const u8* in, size_t size) {
        size_t moved = 0;
        CCCrypt(kCCEncrypt, kCCAlgorithmAES, 0, key.data(), key.size(), iv.data(), in, size, out,
                size, &moved);
    }

private:
    std::array<u8, 16> key{};
    std::array<u8, 16> iv{};
};

class AesCtrCryptor {
public:
    AesCtrCryptor() = default;
    AesCtrCryptor(const u8* key_data, size_t key_size, const u8* iv_data) {
        SetKeyWithIV(key_data, key_size, iv_data);
    }

    void SetKeyWithIV(const u8* key_data, size_t key_size, const u8* iv_data) {
        std::memcpy(key.data(), key_data, key_size);
        std::memcpy(iv.data(), iv_data, iv.size());
        offset = 0;
    }

    void Seek(size_t offset_bytes) {
        offset = offset_bytes;
    }

    void ProcessData(u8* out, const u8* in, size_t size) {
        size_t pos = 0;
        while (pos < size) {
            std::array<u8, 16> counter = iv;
            AddToCounter(counter, offset / 16);

            std::array<u8, 16> stream{};
            size_t moved = 0;
            CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(), key.size(), nullptr,
                    counter.data(), counter.size(), stream.data(), stream.size(), &moved);

            const size_t block_offset = offset % 16;
            const size_t to_xor = std::min<size_t>(16 - block_offset, size - pos);
            for (size_t i = 0; i < to_xor; ++i) {
                out[pos + i] = in[pos + i] ^ stream[block_offset + i];
            }

            offset += to_xor;
            pos += to_xor;
        }
    }

private:
    static void AddToCounter(std::array<u8, 16>& counter, size_t blocks) {
        size_t carry = blocks;
        for (int i = 15; i >= 0 && carry != 0; --i) {
            const size_t sum = static_cast<size_t>(counter[i]) + (carry & 0xFF);
            counter[i] = static_cast<u8>(sum & 0xFF);
            carry = (carry >> 8) + (sum >> 8);
        }
    }

    std::array<u8, 16> key{};
    std::array<u8, 16> iv{};
    size_t offset = 0;
};

#else
using AesCbcDecryptor = CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption;
using AesCbcEncryptor = CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption;
using AesCtrCryptor = CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption;
#endif

} // namespace Common::AESUtil
