// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <array>
#include <algorithm>
#include <cstring>
#include <bearssl.h>
#include <fmt/format.h>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hw/aes/key.h"
#include "core/hw/rsa/rsa.h"

namespace HW::RSA {

constexpr std::size_t SlotSize = 4;
std::array<RsaSlot, SlotSize> rsa_slots;

RsaSlot ticket_wrap_slot;
RsaSlot secure_info_slot;
RsaSlot local_friend_code_seed_slot;

namespace {

constexpr std::array<u8, 19> SHA256_DIGEST_INFO_PREFIX = {
    0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20,
};

std::span<const u8> TrimLeadingZerosSpan(std::span<const u8> value) {
    size_t index = 0;
    while (index + 1 < value.size() && value[index] == 0) {
        ++index;
    }
    return value.subspan(index);
}

std::vector<u8> TrimLeadingZerosVector(std::span<const u8> value) {
    const auto trimmed = TrimLeadingZerosSpan(value);
    return std::vector<u8>(trimmed.begin(), trimmed.end());
}

std::vector<u8> BuildPkcs1v15Sha256EncodedMessage(std::span<const u8> message, size_t modulus_len) {
    std::array<u8, 32> hash{};
    br_sha256_context sha_ctx;
    br_sha256_init(&sha_ctx);
    br_sha256_update(&sha_ctx, message.data(), message.size());
    br_sha256_out(&sha_ctx, hash.data());

    const size_t digest_info_size = SHA256_DIGEST_INFO_PREFIX.size() + hash.size();
    if (modulus_len < digest_info_size + 11) {
        return {};
    }

    std::vector<u8> encoded(modulus_len, 0xFF);
    encoded[0] = 0x00;
    encoded[1] = 0x01;
    const size_t separator_pos = modulus_len - digest_info_size - 1;
    encoded[separator_pos] = 0x00;
    std::memcpy(encoded.data() + separator_pos + 1, SHA256_DIGEST_INFO_PREFIX.data(),
                SHA256_DIGEST_INFO_PREFIX.size());
    std::memcpy(encoded.data() + separator_pos + 1 + SHA256_DIGEST_INFO_PREFIX.size(), hash.data(),
                hash.size());
    return encoded;
}

std::vector<u8> ModularExponentiationImpl(std::span<const u8> base,
                                          std::span<const u8> exponent,
                                          std::span<const u8> modulus) {
    const auto mod_trimmed = TrimLeadingZerosSpan(modulus);
    const auto exp_trimmed = TrimLeadingZerosSpan(exponent);
    if (mod_trimmed.empty() || exp_trimmed.empty()) {
        return {};
    }

    std::vector<u8> x(mod_trimmed.size(), 0);
    const auto base_trimmed = TrimLeadingZerosSpan(base);
    if (base_trimmed.size() > x.size()) {
        return {};
    }
    std::copy(base_trimmed.begin(), base_trimmed.end(), x.end() - base_trimmed.size());

    br_rsa_public_key key{
        .n = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(mod_trimmed.data())),
        .nlen = mod_trimmed.size(),
        .e = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(exp_trimmed.data())),
        .elen = exp_trimmed.size(),
    };

    if (br_rsa_i31_public(x.data(), x.size(), &key) != 1) {
        return {};
    }

    return x;
}

} // namespace

std::vector<u8> RsaSlot::ModularExponentiation(std::span<const u8> message,
                                               int out_size_bytes) const {
    auto result = ModularExponentiationImpl(message, exponent, modulus);
    if (result.empty()) {
        return {};
    }

    if (out_size_bytes == -1) {
        return TrimLeadingZerosVector(result);
    }

    std::vector<u8> resized(out_size_bytes, 0);
    if (result.size() > static_cast<size_t>(out_size_bytes)) {
        std::copy(result.end() - out_size_bytes, result.end(), resized.begin());
    } else {
        std::copy(result.begin(), result.end(), resized.end() - result.size());
    }
    return resized;
}

std::vector<u8> RsaSlot::Sign(std::span<const u8> message) const {
    if (private_d.empty()) {
        LOG_ERROR(HW, "Cannot sign, RSA slot does not have a private key");
        return {};
    }

    const auto modulus_trimmed = TrimLeadingZerosSpan(modulus);
    auto encoded = BuildPkcs1v15Sha256EncodedMessage(message, modulus_trimmed.size());
    if (encoded.empty()) {
        LOG_ERROR(HW_RSA, "Unable to encode PKCS#1 v1.5 message for signing");
        return {};
    }

    auto signature = ModularExponentiationImpl(encoded, private_d, modulus);
    if (signature.empty()) {
        LOG_ERROR(HW_RSA, "RSA signing modular exponentiation failed");
        return {};
    }
    return signature;
}

bool RsaSlot::Verify(std::span<const u8> message, std::span<const u8> signature) const {
    const auto modulus_trimmed = TrimLeadingZerosSpan(modulus);
    if (signature.size() != modulus_trimmed.size()) {
        return false;
    }

    auto expected = BuildPkcs1v15Sha256EncodedMessage(message, modulus_trimmed.size());
    if (expected.empty()) {
        return false;
    }

    auto recovered = ModularExponentiationImpl(signature, exponent, modulus);
    return !recovered.empty() &&
           std::equal(recovered.begin(), recovered.end(), expected.begin(), expected.end());
}

std::vector<u8> HexToVector(const std::string& hex) {
    std::vector<u8> vector(hex.size() / 2);
    for (std::size_t i = 0; i < vector.size(); ++i) {
        vector[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }

    return vector;
}

std::optional<std::pair<std::size_t, char>> ParseKeySlotName(const std::string& full_name) {
    std::size_t slot;
    char type;
    int end;
    if (std::sscanf(full_name.c_str(), "slot0x%zX%c%n", &slot, &type, &end) == 2 &&
        end == static_cast<int>(full_name.size())) {
        return std::make_pair(slot, type);
    } else {
        return std::nullopt;
    }
}

void InitSlots() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    auto s = HW::AES::GetKeysStream();

    std::string mode = "";

    while (!s.eof()) {
        std::string line;
        std::getline(s, line);

        // Ignore empty or commented lines.
        if (line.empty() || line.starts_with("#")) {
            continue;
        }

        if (line.starts_with(":")) {
            mode = line.substr(1);
            continue;
        }

        if (mode != "RSA") {
            continue;
        }

        const auto parts = Common::SplitString(line, '=');
        if (parts.size() != 2) {
            LOG_ERROR(HW_RSA, "Failed to parse {}", line);
            continue;
        }

        const std::string& name = parts[0];

        std::vector<u8> key;
        try {
            key = HexToVector(parts[1]);
        } catch (const std::logic_error& e) {
            LOG_ERROR(HW_RSA, "Invalid key {}: {}", parts[1], e.what());
            continue;
        }

        if (name == "ticketWrapExp") {
            ticket_wrap_slot.SetExponent(key);
            continue;
        }

        if (name == "ticketWrapMod") {
            ticket_wrap_slot.SetModulus(key);
            continue;
        }

        if (name == "secureInfoExp") {
            secure_info_slot.SetExponent(key);
            continue;
        }

        if (name == "secureInfoMod") {
            secure_info_slot.SetModulus(key);
            continue;
        }

        if (name == "lfcsExp") {
            local_friend_code_seed_slot.SetExponent(key);
            continue;
        }

        if (name == "lfcsMod") {
            local_friend_code_seed_slot.SetModulus(key);
            continue;
        }

        const auto key_slot = ParseKeySlotName(name);
        if (!key_slot) {
            LOG_ERROR(HW_RSA, "Invalid key name '{}'", name);
            continue;
        }

        if (key_slot->first >= SlotSize) {
            LOG_ERROR(HW_RSA, "Out of range key slot ID {:#X}", key_slot->first);
            continue;
        }

        switch (key_slot->second) {
        case 'X':
            rsa_slots.at(key_slot->first).SetExponent(key);
            break;
        case 'M':
            rsa_slots.at(key_slot->first).SetModulus(key);
            break;
        case 'P':
            rsa_slots.at(key_slot->first).SetPrivateD(key);
            break;
        default:
            LOG_ERROR(HW_RSA, "Invalid key type '{}'", key_slot->second);
            break;
        }
    }
}

static RsaSlot empty_slot;
const RsaSlot& GetSlot(std::size_t slot_id) {
    if (slot_id >= rsa_slots.size())
        return empty_slot;
    return rsa_slots[slot_id];
}

const RsaSlot& GetTicketWrapSlot() {
    return ticket_wrap_slot;
}

const RsaSlot& GetSecureInfoSlot() {
    return secure_info_slot;
}

const RsaSlot& GetLocalFriendCodeSeedSlot() {
    return local_friend_code_seed_slot;
}

} // namespace HW::RSA
