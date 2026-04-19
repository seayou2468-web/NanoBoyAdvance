// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <sstream>
#include <vector>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif
#include "../../../common/common_paths.h"
#include "../../../common/file_util.h"
#include "../../../common/logging/log.h"
#include "../../../common/string_util.h"
#include "../aes/key.h"
#include "rsa.h"

namespace HW::RSA {

namespace {

#if defined(__APPLE__)
std::vector<u8> EncodeAsn1Length(std::size_t length) {
    if (length < 0x80) {
        return {static_cast<u8>(length)};
    }

    std::vector<u8> encoded;
    while (length != 0) {
        encoded.push_back(static_cast<u8>(length & 0xFF));
        length >>= 8;
    }
    std::reverse(encoded.begin(), encoded.end());
    encoded.insert(encoded.begin(), static_cast<u8>(0x80 | encoded.size()));
    return encoded;
}

void AppendAsn1Integer(std::vector<u8>& out, std::span<const u8> integer_bytes) {
    std::size_t first_non_zero = 0;
    while (first_non_zero + 1 < integer_bytes.size() && integer_bytes[first_non_zero] == 0) {
        ++first_non_zero;
    }
    std::span<const u8> trimmed = integer_bytes.subspan(first_non_zero);

    out.push_back(0x02); // INTEGER
    const bool needs_sign_padding = !trimmed.empty() && (trimmed.front() & 0x80) != 0;
    const std::size_t payload_size = trimmed.size() + (needs_sign_padding ? 1 : 0);
    const std::vector<u8> len = EncodeAsn1Length(payload_size);
    out.insert(out.end(), len.begin(), len.end());
    if (needs_sign_padding) {
        out.push_back(0x00);
    }
    out.insert(out.end(), trimmed.begin(), trimmed.end());
}

std::vector<u8> BuildRsaPublicKeyDer(std::span<const u8> modulus, std::span<const u8> exponent) {
    std::vector<u8> seq_payload;
    AppendAsn1Integer(seq_payload, modulus);
    AppendAsn1Integer(seq_payload, exponent);

    std::vector<u8> der;
    der.push_back(0x30); // SEQUENCE
    const std::vector<u8> seq_len = EncodeAsn1Length(seq_payload.size());
    der.insert(der.end(), seq_len.begin(), seq_len.end());
    der.insert(der.end(), seq_payload.begin(), seq_payload.end());
    return der;
}

class ScopedCFTypeRef final {
public:
    explicit ScopedCFTypeRef(CFTypeRef value = nullptr) : value(value) {}
    ~ScopedCFTypeRef() {
        if (value != nullptr) {
            CFRelease(value);
        }
    }

    ScopedCFTypeRef(const ScopedCFTypeRef&) = delete;
    ScopedCFTypeRef& operator=(const ScopedCFTypeRef&) = delete;

    ScopedCFTypeRef(ScopedCFTypeRef&& other) noexcept : value(other.value) {
        other.value = nullptr;
    }
    ScopedCFTypeRef& operator=(ScopedCFTypeRef&& other) noexcept {
        if (this != &other) {
            if (value != nullptr) {
                CFRelease(value);
            }
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }

    template <typename T>
    T As() const {
        return reinterpret_cast<T>(value);
    }

private:
    CFTypeRef value;
};

SecKeyRef CreateSecRsaPublicKey(std::span<const u8> modulus, std::span<const u8> exponent) {
    const std::vector<u8> der = BuildRsaPublicKeyDer(modulus, exponent);
    ScopedCFTypeRef key_data(CFDataCreate(kCFAllocatorDefault, der.data(), der.size()));
    if (key_data.As<CFDataRef>() == nullptr) {
        return nullptr;
    }

    const int key_size_bits = static_cast<int>(modulus.size() * 8);
    const void* keys[] = {kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits};
    ScopedCFTypeRef key_size(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_size_bits));
    const void* values[] = {kSecAttrKeyTypeRSA, kSecAttrKeyClassPublic, key_size.As<CFNumberRef>()};
    ScopedCFTypeRef attrs(
        CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks));
    if (attrs.As<CFDictionaryRef>() == nullptr) {
        return nullptr;
    }

    CFErrorRef error = nullptr;
    SecKeyRef key = SecKeyCreateWithData(key_data.As<CFDataRef>(), attrs.As<CFDictionaryRef>(), &error);
    if (error != nullptr) {
        CFRelease(error);
    }
    return key;
}

std::vector<u8> BuildRsaPrivateKeyDer(std::span<const u8> modulus, std::span<const u8> exponent,
                                      std::span<const u8> private_d) {
    std::vector<u8> seq_payload;
    static constexpr std::array<u8, 1> zero = {0x00};

    AppendAsn1Integer(seq_payload, zero);       // version
    AppendAsn1Integer(seq_payload, modulus);    // n
    AppendAsn1Integer(seq_payload, exponent);   // e
    AppendAsn1Integer(seq_payload, private_d);  // d
    AppendAsn1Integer(seq_payload, zero);       // p
    AppendAsn1Integer(seq_payload, zero);       // q
    AppendAsn1Integer(seq_payload, zero);       // d mod (p - 1)
    AppendAsn1Integer(seq_payload, zero);       // d mod (q - 1)
    AppendAsn1Integer(seq_payload, zero);       // q^-1 mod p

    std::vector<u8> der;
    der.push_back(0x30); // SEQUENCE
    const std::vector<u8> seq_len = EncodeAsn1Length(seq_payload.size());
    der.insert(der.end(), seq_len.begin(), seq_len.end());
    der.insert(der.end(), seq_payload.begin(), seq_payload.end());
    return der;
}

SecKeyRef CreateSecRsaPrivateKey(std::span<const u8> modulus, std::span<const u8> exponent,
                                 std::span<const u8> private_d) {
    const std::vector<u8> der = BuildRsaPrivateKeyDer(modulus, exponent, private_d);
    ScopedCFTypeRef key_data(CFDataCreate(kCFAllocatorDefault, der.data(), der.size()));
    if (key_data.As<CFDataRef>() == nullptr) {
        return nullptr;
    }

    const int key_size_bits = static_cast<int>(modulus.size() * 8);
    const void* keys[] = {kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits};
    ScopedCFTypeRef key_size(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_size_bits));
    const void* values[] = {kSecAttrKeyTypeRSA, kSecAttrKeyClassPrivate, key_size.As<CFNumberRef>()};
    ScopedCFTypeRef attrs(
        CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks));
    if (attrs.As<CFDictionaryRef>() == nullptr) {
        return nullptr;
    }

    CFErrorRef error = nullptr;
    SecKeyRef key = SecKeyCreateWithData(key_data.As<CFDataRef>(), attrs.As<CFDictionaryRef>(), &error);
    if (error != nullptr) {
        CFRelease(error);
    }
    return key;
}
#endif

} // namespace

constexpr std::size_t SlotSize = 4;
std::array<RsaSlot, SlotSize> rsa_slots;

RsaSlot ticket_wrap_slot;
RsaSlot secure_info_slot;
RsaSlot local_friend_code_seed_slot;

std::vector<u8> RsaSlot::ModularExponentiation(std::span<const u8> message,
                                               int out_size_bytes) const {
#if defined(__APPLE__)
    ScopedCFTypeRef public_key(CreateSecRsaPublicKey(modulus, exponent));
    if (public_key.As<SecKeyRef>() == nullptr) {
        LOG_ERROR(HW_RSA, "Failed to create SecKey public key");
        return {};
    }

    const auto block_size = static_cast<std::size_t>(SecKeyGetBlockSize(public_key.As<SecKeyRef>()));
    std::vector<u8> block(block_size, 0);
    const std::size_t input_copy_size = std::min(message.size(), block.size());
    std::copy(message.end() - input_copy_size, message.end(), block.end() - input_copy_size);

    ScopedCFTypeRef message_data(CFDataCreate(kCFAllocatorDefault, block.data(), block.size()));
    if (message_data.As<CFDataRef>() == nullptr) {
        return {};
    }

    CFErrorRef error = nullptr;
    ScopedCFTypeRef result_data(SecKeyCreateEncryptedData(public_key.As<SecKeyRef>(),
                                                          kSecKeyAlgorithmRSAEncryptionRaw,
                                                          message_data.As<CFDataRef>(), &error));
    if (error != nullptr) {
        CFRelease(error);
    }
    if (result_data.As<CFDataRef>() == nullptr) {
        LOG_ERROR(HW_RSA, "SecKeyCreateEncryptedData failed");
        return {};
    }

    const auto* result_ptr = CFDataGetBytePtr(result_data.As<CFDataRef>());
    const std::size_t result_len = static_cast<std::size_t>(CFDataGetLength(result_data.As<CFDataRef>()));
    const std::size_t output_size =
        out_size_bytes == -1 ? result_len : static_cast<std::size_t>(out_size_bytes);
    std::vector<u8> out(output_size, 0);
    if (result_len > out.size()) {
        std::copy(result_ptr + (result_len - out.size()), result_ptr + result_len, out.begin());
    } else {
        std::copy(result_ptr, result_ptr + result_len, out.end() - result_len);
    }
    return out;
#else
    LOG_ERROR(HW_RSA, "RSA modular exponentiation is only supported on Apple targets");
    return {};
#endif
}

std::vector<u8> RsaSlot::Sign(std::span<const u8> message) const {
    if (private_d.empty()) {
        LOG_ERROR(HW, "Cannot sign, RSA slot does not have a private key");
        return {};
    }

#if defined(__APPLE__)
    ScopedCFTypeRef private_key(CreateSecRsaPrivateKey(modulus, exponent, private_d));
    if (private_key.As<SecKeyRef>() == nullptr) {
        LOG_ERROR(HW_RSA, "Failed to create SecKey private key");
        return {};
    }

    ScopedCFTypeRef message_data(CFDataCreate(kCFAllocatorDefault, message.data(), message.size()));
    if (message_data.As<CFDataRef>() == nullptr) {
        return {};
    }

    CFErrorRef error = nullptr;
    ScopedCFTypeRef signature_data(
        SecKeyCreateSignature(private_key.As<SecKeyRef>(),
                              kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
                              message_data.As<CFDataRef>(), &error));
    if (error != nullptr) {
        CFRelease(error);
    }
    if (signature_data.As<CFDataRef>() == nullptr) {
        LOG_ERROR(HW_RSA, "SecKeyCreateSignature failed");
        return {};
    }

    const auto* sig_ptr = CFDataGetBytePtr(signature_data.As<CFDataRef>());
    const std::size_t sig_len = static_cast<std::size_t>(CFDataGetLength(signature_data.As<CFDataRef>()));
    return std::vector<u8>(sig_ptr, sig_ptr + sig_len);
#else
    LOG_ERROR(HW_RSA, "RSA sign is only supported on Apple targets");
    return {};
#endif
}

bool RsaSlot::Verify(std::span<const u8> message, std::span<const u8> signature) const {
#if defined(__APPLE__)
    ScopedCFTypeRef public_key(CreateSecRsaPublicKey(modulus, exponent));
    if (public_key.As<SecKeyRef>() != nullptr) {
        ScopedCFTypeRef message_data(CFDataCreate(kCFAllocatorDefault, message.data(), message.size()));
        ScopedCFTypeRef signature_data(
            CFDataCreate(kCFAllocatorDefault, signature.data(), signature.size()));
        if (message_data.As<CFDataRef>() != nullptr && signature_data.As<CFDataRef>() != nullptr) {
            const bool ok = SecKeyVerifySignature(public_key.As<SecKeyRef>(),
                                                  kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
                                                  message_data.As<CFDataRef>(),
                                                  signature_data.As<CFDataRef>(), nullptr);
            if (ok) {
                return true;
            }
        }
    }
    return false;
#else
    LOG_ERROR(HW_RSA, "RSA verify is only supported on Apple targets");
    return false;
#endif
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
