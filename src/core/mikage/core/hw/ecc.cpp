// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <type_traits>
#if defined(__APPLE__)
#include <Security/Security.h>
#else
#error "Mikage ECC backend now requires Apple Security.framework"
#endif
#include "../../common/assert.h"
#include "../../common/logging/log.h"
#include "../../common/secure_random.h"
#include "../../common/string_util.h"
#include "aes/key.h"
#include "ecc.h"

namespace HW::ECC {

PublicKey root_public;

namespace {

constexpr std::size_t P256_SCALAR_SIZE = 32;
constexpr std::size_t P256_PUBLIC_SIZE = 65; // 0x04 || X(32) || Y(32)

using CFTypePtr = std::unique_ptr<std::remove_pointer_t<CFTypeRef>, decltype(&CFRelease)>;

CFTypePtr MakeCFData(const u8* data, std::size_t size) {
    return CFTypePtr(CFDataCreate(kCFAllocatorDefault, data, static_cast<CFIndex>(size)),
                     &CFRelease);
}

std::array<u8, P256_SCALAR_SIZE> ToP256Scalar(const std::array<u8, INT_SIZE>& value) {
    std::array<u8, P256_SCALAR_SIZE> out{};
    std::memcpy(out.data() + (P256_SCALAR_SIZE - INT_SIZE), value.data(), INT_SIZE);
    return out;
}

std::array<u8, INT_SIZE> FromP256Scalar(const u8* value, std::size_t size) {
    std::array<u8, INT_SIZE> out{};
    if (size >= INT_SIZE) {
        std::memcpy(out.data(), value + (size - INT_SIZE), INT_SIZE);
    } else {
        std::memcpy(out.data() + (INT_SIZE - size), value, size);
    }
    return out;
}

std::array<u8, P256_PUBLIC_SIZE> ToP256Public(const PublicKey& public_key) {
    std::array<u8, P256_PUBLIC_SIZE> out{};
    out[0] = 0x04;
    std::memcpy(out.data() + 1 + (P256_SCALAR_SIZE - INT_SIZE), public_key.x.data(), INT_SIZE);
    std::memcpy(out.data() + 1 + P256_SCALAR_SIZE + (P256_SCALAR_SIZE - INT_SIZE),
                public_key.y.data(), INT_SIZE);
    return out;
}

PublicKey FromP256Public(const u8* value, std::size_t size) {
    PublicKey out{};
    if (size < P256_PUBLIC_SIZE || value[0] != 0x04) {
        return out;
    }
    std::memcpy(out.x.data(), value + 1 + (P256_SCALAR_SIZE - INT_SIZE), INT_SIZE);
    std::memcpy(out.y.data(), value + 1 + P256_SCALAR_SIZE + (P256_SCALAR_SIZE - INT_SIZE), INT_SIZE);
    return out;
}

CFTypePtr CreateECAttributes(bool is_private) {
    const void* keys[] = {
        kSecAttrKeyType,
        kSecAttrKeyClass,
        kSecAttrKeySizeInBits,
    };
    const int key_bits = 256;
    CFNumberRef bit_count = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_bits);
    const void* values[] = {
        kSecAttrKeyTypeECSECPrimeRandom,
        is_private ? kSecAttrKeyClassPrivate : kSecAttrKeyClassPublic,
        bit_count,
    };

    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFRelease(bit_count);
    return CFTypePtr(attrs, &CFRelease);
}

SecKeyRef CreatePrivateSecKey(const PrivateKey& private_key) {
    const auto scalar = ToP256Scalar(private_key.x);
    auto data = MakeCFData(scalar.data(), scalar.size());
    auto attrs = CreateECAttributes(true);
    if (!data || !attrs) {
        return nullptr;
    }

    CFErrorRef error = nullptr;
    SecKeyRef key = SecKeyCreateWithData(static_cast<CFDataRef>(data.get()),
                                         static_cast<CFDictionaryRef>(attrs.get()), &error);
    if (error) {
        CFRelease(error);
    }
    return key;
}

SecKeyRef CreatePublicSecKey(const PublicKey& public_key) {
    const auto pub = ToP256Public(public_key);
    auto data = MakeCFData(pub.data(), pub.size());
    auto attrs = CreateECAttributes(false);
    if (!data || !attrs) {
        return nullptr;
    }

    CFErrorRef error = nullptr;
    SecKeyRef key = SecKeyCreateWithData(static_cast<CFDataRef>(data.get()),
                                         static_cast<CFDictionaryRef>(attrs.get()), &error);
    if (error) {
        CFRelease(error);
    }
    return key;
}

std::vector<u8> ParseDerEcdsaSignature(CFDataRef der_data) {
    std::vector<u8> out(INT_SIZE * 2, 0);
    if (!der_data) {
        return out;
    }

    const u8* der = CFDataGetBytePtr(der_data);
    const std::size_t size = static_cast<std::size_t>(CFDataGetLength(der_data));
    if (!der || size < 8 || der[0] != 0x30) {
        return out;
    }

    std::size_t pos = 2;
    if (pos >= size || der[pos] != 0x02) {
        return out;
    }
    ++pos;
    if (pos >= size) {
        return out;
    }
    std::size_t r_len = der[pos++];
    if (pos + r_len > size || pos >= size || der[pos + r_len] != 0x02) {
        return out;
    }
    const u8* r_ptr = der + pos;
    pos += r_len + 1;
    if (pos >= size) {
        return out;
    }
    std::size_t s_len = der[pos++];
    if (pos + s_len > size) {
        return out;
    }
    const u8* s_ptr = der + pos;

    auto copy_component = [](u8* dst, const u8* src, std::size_t len) {
        if (len > INT_SIZE) {
            src += len - INT_SIZE;
            len = INT_SIZE;
        }
        std::memcpy(dst + (INT_SIZE - len), src, len);
    };

    copy_component(out.data(), r_ptr, r_len);
    copy_component(out.data() + INT_SIZE, s_ptr, s_len);
    return out;
}

CFTypePtr BuildDerEcdsaSignature(const Signature& sig) {
    auto trim_leading_zeros = [](const std::array<u8, INT_SIZE>& v) {
        std::size_t offset = 0;
        while (offset < v.size() - 1 && v[offset] == 0) {
            ++offset;
        }
        return std::pair<const u8*, std::size_t>(v.data() + offset, v.size() - offset);
    };

    auto [r_ptr, r_len] = trim_leading_zeros(sig.r);
    auto [s_ptr, s_len] = trim_leading_zeros(sig.s);

    std::vector<u8> der;
    der.reserve(2 + 2 + r_len + 2 + s_len + 2);
    der.push_back(0x30);
    der.push_back(static_cast<u8>(2 + r_len + 2 + s_len));
    der.push_back(0x02);
    der.push_back(static_cast<u8>(r_len));
    der.insert(der.end(), r_ptr, r_ptr + r_len);
    der.push_back(0x02);
    der.push_back(static_cast<u8>(s_len));
    der.insert(der.end(), s_ptr, s_ptr + s_len);

    return MakeCFData(der.data(), der.size());
}

} // namespace

std::vector<u8> HexToVector(const std::string& hex) {
    std::vector<u8> vector(hex.size() / 2);
    for (std::size_t i = 0; i < vector.size(); ++i) {
        vector[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }

    return vector;
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

        if (mode != "ECC") {
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

        if (name == "rootPublicXY") {
            memcpy(root_public.xy.data(), key.data(), std::min(root_public.xy.size(), key.size()));
            continue;
        }
    }
}

PrivateKey CreateECCPrivateKey(std::span<const u8> private_key_x, bool /*fix_up*/) {
    PrivateKey ret{};
    std::memcpy(ret.x.data(), private_key_x.data(), std::min(ret.x.size(), private_key_x.size()));
    return ret;
}

PublicKey CreateECCPublicKey(std::span<const u8> public_key_xy) {
    ASSERT_MSG(public_key_xy.size() <= sizeof(PublicKey::xy), "Invalid public key length");

    PublicKey ret;
    memcpy(ret.xy.data(), public_key_xy.data(), ret.xy.size());
    return ret;
}

Signature CreateECCSignature(std::span<const u8> signature_rs) {
    ASSERT_MSG(signature_rs.size() <= sizeof(Signature::rs), "Invalid signature length");

    Signature ret;
    memcpy(ret.rs.data(), signature_rs.data(), ret.rs.size());
    return ret;
}

PublicKey MakePublicKey(const PrivateKey& private_key) {
    PublicKey ret{};
    SecKeyRef private_sec_key = CreatePrivateSecKey(private_key);
    if (!private_sec_key) {
        LOG_ERROR(HW, "Failed to create SecKey private key");
        return ret;
    }

    SecKeyRef public_sec_key = SecKeyCopyPublicKey(private_sec_key);
    CFRelease(private_sec_key);
    if (!public_sec_key) {
        LOG_ERROR(HW, "Failed to derive public key");
        return ret;
    }

    CFErrorRef error = nullptr;
    CFDataRef pub_data = SecKeyCopyExternalRepresentation(public_sec_key, &error);
    CFRelease(public_sec_key);
    if (error) {
        CFRelease(error);
    }
    if (!pub_data) {
        LOG_ERROR(HW, "Failed to export public key");
        return ret;
    }

    ret = FromP256Public(CFDataGetBytePtr(pub_data), static_cast<std::size_t>(CFDataGetLength(pub_data)));
    CFRelease(pub_data);
    return ret;
}

std::pair<PrivateKey, PublicKey> GenerateKeyPair() {
    PrivateKey private_key{};
    PublicKey public_key{};

    const void* keys[] = {kSecAttrKeyType, kSecAttrKeySizeInBits};
    const int bits = 256;
    CFNumberRef bits_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bits);
    const void* values[] = {kSecAttrKeyTypeECSECPrimeRandom, bits_num};
    CFDictionaryRef params = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
    CFRelease(bits_num);

    CFErrorRef error = nullptr;
    SecKeyRef private_sec_key = SecKeyCreateRandomKey(params, &error);
    CFRelease(params);
    if (error) {
        CFRelease(error);
    }
    if (!private_sec_key) {
        LOG_ERROR(HW, "Failed to generate EC key pair");
        return {private_key, public_key};
    }

    CFDataRef private_data = SecKeyCopyExternalRepresentation(private_sec_key, &error);
    if (error) {
        CFRelease(error);
    }
    if (private_data) {
        auto scalar = FromP256Scalar(CFDataGetBytePtr(private_data),
                                     static_cast<std::size_t>(CFDataGetLength(private_data)));
        private_key.x = scalar;
        CFRelease(private_data);
    }

    SecKeyRef public_sec_key = SecKeyCopyPublicKey(private_sec_key);
    CFRelease(private_sec_key);
    if (public_sec_key) {
        CFDataRef public_data = SecKeyCopyExternalRepresentation(public_sec_key, &error);
        if (error) {
            CFRelease(error);
        }
        if (public_data) {
            public_key = FromP256Public(CFDataGetBytePtr(public_data),
                                        static_cast<std::size_t>(CFDataGetLength(public_data)));
            CFRelease(public_data);
        }
        CFRelease(public_sec_key);
    }

    return std::make_pair(private_key, public_key);
}

Signature Sign(std::span<const u8> data, PrivateKey private_key) {
    Signature ret{};
    SecKeyRef private_sec_key = CreatePrivateSecKey(private_key);
    if (!private_sec_key) {
        LOG_ERROR(HW, "Failed to create private key for signing");
        return ret;
    }

    auto input_data = MakeCFData(data.data(), data.size());
    if (!input_data) {
        CFRelease(private_sec_key);
        return ret;
    }

    CFErrorRef error = nullptr;
    CFDataRef signature = SecKeyCreateSignature(private_sec_key,
                                                kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                                static_cast<CFDataRef>(input_data.get()), &error);
    CFRelease(private_sec_key);
    if (error) {
        CFRelease(error);
    }
    if (!signature) {
        LOG_ERROR(HW, "SecKeyCreateSignature failed");
        return ret;
    }

    const auto parsed = ParseDerEcdsaSignature(signature);
    std::memcpy(ret.rs.data(), parsed.data(), ret.rs.size());
    CFRelease(signature);
    return ret;
}

bool Verify(std::span<const u8> data, Signature signature, PublicKey public_key) {
    SecKeyRef public_sec_key = CreatePublicSecKey(public_key);
    if (!public_sec_key) {
        LOG_ERROR(HW, "Failed to construct public key for verify");
        return false;
    }

    auto input_data = MakeCFData(data.data(), data.size());
    auto der_sig = BuildDerEcdsaSignature(signature);
    if (!input_data || !der_sig) {
        CFRelease(public_sec_key);
        return false;
    }

    CFErrorRef error = nullptr;
    const bool result = SecKeyVerifySignature(public_sec_key,
                                              kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                              static_cast<CFDataRef>(input_data.get()),
                                              static_cast<CFDataRef>(der_sig.get()), &error);
    CFRelease(public_sec_key);
    if (error) {
        CFRelease(error);
    }
    return result;
}

std::vector<u8> Agree(PrivateKey private_key, PublicKey others_public_key) {
    SecKeyRef private_sec_key = CreatePrivateSecKey(private_key);
    SecKeyRef public_sec_key = CreatePublicSecKey(others_public_key);
    if (!private_sec_key || !public_sec_key) {
        if (private_sec_key) {
            CFRelease(private_sec_key);
        }
        if (public_sec_key) {
            CFRelease(public_sec_key);
        }
        LOG_ERROR(HW, "Failed to construct key material for ECDH");
        return {};
    }

    CFErrorRef error = nullptr;
    CFDataRef shared = SecKeyCopyKeyExchangeResult(private_sec_key,
                                                   kSecKeyAlgorithmECDHKeyExchangeStandard,
                                                   public_sec_key, nullptr, &error);
    CFRelease(private_sec_key);
    CFRelease(public_sec_key);
    if (error) {
        CFRelease(error);
    }
    if (!shared) {
        LOG_ERROR(HW, "SecKeyCopyKeyExchangeResult failed");
        return {};
    }

    const u8* ptr = CFDataGetBytePtr(shared);
    const std::size_t len = static_cast<std::size_t>(CFDataGetLength(shared));
    std::vector<u8> agreement(ptr, ptr + len);
    CFRelease(shared);
    return agreement;
}

const PublicKey& GetRootPublicKey() {
    return root_public;
}

} // namespace HW::ECC
