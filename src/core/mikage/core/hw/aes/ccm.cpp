// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include "../../../common/commoncrypto_aes.h"
#include "../../../common/alignment.h"
#include "../../../common/logging/log.h"
#include "ccm.h"
#include "key.h"

namespace HW::AES {

namespace {

using Common::Crypto::AESBlock;
constexpr std::size_t kLengthFieldSize = 15 - CCM_NONCE_SIZE;
static_assert(kLengthFieldSize == 3, "Unexpected CCM length field size");

void EncodeLength(std::uint64_t value, std::span<u8, kLengthFieldSize> out) {
    for (std::size_t i = 0; i < out.size(); ++i) {
        const auto shift = static_cast<unsigned>((out.size() - 1 - i) * 8);
        out[i] = static_cast<u8>((value >> shift) & 0xFF);
    }
}

AESBlock MakeB0(const CCMNonce& nonce, std::size_t auth_message_length) {
    AESBlock b0{};
    constexpr u8 flags = static_cast<u8>(((CCM_MAC_SIZE - 2) / 2) << 3) |
                         static_cast<u8>(kLengthFieldSize - 1);
    b0[0] = flags;
    std::memcpy(b0.data() + 1, nonce.data(), nonce.size());
    EncodeLength(auth_message_length, std::span<u8, kLengthFieldSize>(b0.data() + 1 + nonce.size(),
                                                                       kLengthFieldSize));
    return b0;
}

AESBlock MakeCtrBlock(const CCMNonce& nonce, std::uint64_t counter) {
    AESBlock ctr{};
    ctr[0] = static_cast<u8>(kLengthFieldSize - 1);
    std::memcpy(ctr.data() + 1, nonce.data(), nonce.size());
    EncodeLength(counter, std::span<u8, kLengthFieldSize>(ctr.data() + 1 + nonce.size(),
                                                          kLengthFieldSize));
    return ctr;
}

bool Compute3DSCcmMac(std::span<const u8> message, const CCMNonce& nonce, const AESKey& key,
                      AESBlock& mac_out) {
    AESBlock mac{};
    const std::size_t aligned_len = Common::AlignUp(message.size(), AES_BLOCK_SIZE);
    AESBlock b0 = MakeB0(nonce, aligned_len);
    for (std::size_t i = 0; i < AES_BLOCK_SIZE; ++i) {
        mac[i] ^= b0[i];
    }
    if (!Common::Crypto::AESECBEncryptBlock(mac, key, mac)) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < message.size()) {
        AESBlock block{};
        const std::size_t chunk = std::min(AES_BLOCK_SIZE, message.size() - offset);
        std::memcpy(block.data(), message.data() + offset, chunk);
        for (std::size_t i = 0; i < AES_BLOCK_SIZE; ++i) {
            mac[i] ^= block[i];
        }
        if (!Common::Crypto::AESECBEncryptBlock(mac, key, mac)) {
            return false;
        }
        offset += chunk;
    }

    mac_out = mac;
    return true;
}

bool TransformCtr(std::span<const u8> input, std::span<u8> output, const CCMNonce& nonce,
                  const AESKey& key) {
    std::size_t offset = 0;
    std::uint64_t counter = 1;
    while (offset < input.size()) {
        AESBlock stream_block{};
        const AESBlock ctr = MakeCtrBlock(nonce, counter++);
        if (!Common::Crypto::AESECBEncryptBlock(ctr, key, stream_block)) {
            return false;
        }

        const std::size_t chunk = std::min(AES_BLOCK_SIZE, input.size() - offset);
        for (std::size_t i = 0; i < chunk; ++i) {
            output[offset + i] = static_cast<u8>(input[offset + i] ^ stream_block[i]);
        }
        offset += chunk;
    }

    return true;
}

bool BuildTag(std::span<const u8> message, const CCMNonce& nonce, const AESKey& key,
              std::span<u8, CCM_MAC_SIZE> tag_out) {
    AESBlock mac{};
    if (!Compute3DSCcmMac(message, nonce, key, mac)) {
        return false;
    }

    AESBlock s0{};
    const AESBlock ctr0 = MakeCtrBlock(nonce, 0);
    if (!Common::Crypto::AESECBEncryptBlock(ctr0, key, s0)) {
        return false;
    }

    for (std::size_t i = 0; i < CCM_MAC_SIZE; ++i) {
        tag_out[i] = static_cast<u8>(mac[i] ^ s0[i]);
    }
    return true;
}

} // namespace

std::vector<u8> EncryptSignCCM(std::span<const u8> pdata, const CCMNonce& nonce,
                               std::size_t slot_id) {
    if (!IsNormalKeyAvailable(slot_id)) {
        LOG_ERROR(HW_AES, "Key slot {} not available. Will use zero key.", slot_id);
    }
    const AESKey normal = GetNormalKey(slot_id);
    std::vector<u8> cipher(pdata.size() + CCM_MAC_SIZE);

    if (!TransformCtr(pdata, std::span<u8>(cipher.data(), pdata.size()), nonce, normal)) {
        LOG_ERROR(HW_AES, "FAILED with platform AES CTR transform");
        return {};
    }
    if (!BuildTag(pdata, nonce, normal,
                  std::span<u8, CCM_MAC_SIZE>(cipher.data() + pdata.size(), CCM_MAC_SIZE))) {
        LOG_ERROR(HW_AES, "FAILED with platform tag generation");
        return {};
    }
    return cipher;
}

std::vector<u8> DecryptVerifyCCM(std::span<const u8> cipher, const CCMNonce& nonce,
                                 std::size_t slot_id) {
    if (cipher.size() < CCM_MAC_SIZE) {
        LOG_ERROR(HW_AES, "FAILED: invalid CCM payload size {}", cipher.size());
        return {};
    }
    if (!IsNormalKeyAvailable(slot_id)) {
        LOG_ERROR(HW_AES, "Key slot {} not available. Will use zero key.", slot_id);
    }
    const AESKey normal = GetNormalKey(slot_id);
    const std::size_t pdata_size = cipher.size() - CCM_MAC_SIZE;
    std::vector<u8> pdata(pdata_size);

    if (!TransformCtr(std::span<const u8>(cipher.data(), pdata_size), pdata, nonce, normal)) {
        LOG_ERROR(HW_AES, "FAILED with platform AES CTR transform");
        return {};
    }

    std::array<u8, CCM_MAC_SIZE> expected_tag{};
    if (!BuildTag(pdata, nonce, normal, expected_tag)) {
        LOG_ERROR(HW_AES, "FAILED with platform tag generation");
        return {};
    }
    if (!std::equal(expected_tag.begin(), expected_tag.end(), cipher.begin() + pdata_size)) {
        LOG_ERROR(HW_AES, "FAILED");
        return {};
    }
    return pdata;
}

} // namespace HW::AES
