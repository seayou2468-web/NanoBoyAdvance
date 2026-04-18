// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include "common/assert.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/secure_random.h"
#include "common/string_util.h"
#include "core/hw/aes/key.h"
#include "core/hw/ecc.h"

namespace HW::ECC {

PublicKey root_public;

namespace {
using EcGroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;
using EcPointPtr = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
using EcKeyPtr = std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)>;
using EcSigPtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
using BnPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using BnCtxPtr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;

EcGroupPtr CreateGroup() {
    return EcGroupPtr(EC_GROUP_new_by_curve_name(NID_sect233r1), &EC_GROUP_free);
}

BnPtr CreateBigNum(std::span<const u8> data) {
    if (data.empty()) {
        return BnPtr(BN_new(), &BN_free);
    }
    return BnPtr(BN_bin2bn(data.data(), static_cast<int>(data.size()), nullptr), &BN_free);
}

EcPointPtr CreatePoint(const EC_GROUP* group, const PublicKey& public_key) {
    EcPointPtr point(EC_POINT_new(group), &EC_POINT_free);
    BnCtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
    BnPtr x = CreateBigNum(public_key.x);
    BnPtr y = CreateBigNum(public_key.y);
    if (!point || !ctx || !x || !y) {
        return EcPointPtr(nullptr, &EC_POINT_free);
    }
    if (EC_POINT_set_affine_coordinates(group, point.get(), x.get(), y.get(), ctx.get()) != 1) {
        return EcPointPtr(nullptr, &EC_POINT_free);
    }
    return point;
}

EcKeyPtr CreatePrivateEcKey(const PrivateKey& private_key) {
    EcGroupPtr group = CreateGroup();
    if (!group) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    EcKeyPtr key(EC_KEY_new(), &EC_KEY_free);
    BnPtr priv = CreateBigNum(private_key.x);
    if (!key || !priv) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    if (EC_KEY_set_group(key.get(), group.get()) != 1 || EC_KEY_set_private_key(key.get(), priv.get()) != 1) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }

    EcPointPtr pub(EC_POINT_new(group.get()), &EC_POINT_free);
    BnCtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
    if (!pub || !ctx) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    if (EC_POINT_mul(group.get(), pub.get(), priv.get(), nullptr, nullptr, ctx.get()) != 1 ||
        EC_KEY_set_public_key(key.get(), pub.get()) != 1) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    return key;
}

EcKeyPtr CreatePublicEcKey(const PublicKey& public_key) {
    EcGroupPtr group = CreateGroup();
    if (!group) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    EcKeyPtr key(EC_KEY_new(), &EC_KEY_free);
    if (!key) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    if (EC_KEY_set_group(key.get(), group.get()) != 1) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    EcPointPtr point = CreatePoint(group.get(), public_key);
    if (!point || EC_KEY_set_public_key(key.get(), point.get()) != 1) {
        return EcKeyPtr(nullptr, &EC_KEY_free);
    }
    return key;
}

PublicKey GetPublicKeyFromEcKey(const EC_KEY* key) {
    PublicKey public_key{};
    const EC_GROUP* group = EC_KEY_get0_group(key);
    const EC_POINT* point = EC_KEY_get0_public_key(key);
    BnCtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
    BnPtr x(BN_new(), &BN_free);
    BnPtr y(BN_new(), &BN_free);
    if (!group || !point || !ctx || !x || !y) {
        return public_key;
    }
    if (EC_POINT_get_affine_coordinates(group, point, x.get(), y.get(), ctx.get()) != 1) {
        return public_key;
    }
    BN_bn2binpad(x.get(), public_key.x.data(), static_cast<int>(public_key.x.size()));
    BN_bn2binpad(y.get(), public_key.y.data(), static_cast<int>(public_key.y.size()));
    return public_key;
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

PrivateKey CreateECCPrivateKey(std::span<const u8> private_key_x, bool fix_up) {
    PrivateKey ret{};
    BnPtr priv = CreateBigNum(private_key_x);
    if (!priv) {
        return ret;
    }
    if (fix_up) {
        EcGroupPtr group = CreateGroup();
        BnCtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
        BnPtr order(BN_new(), &BN_free);
        if (group && ctx && order && EC_GROUP_get_order(group.get(), order.get(), ctx.get()) == 1) {
            BN_mod(priv.get(), priv.get(), order.get(), ctx.get());
        }
    }
    BN_bn2binpad(priv.get(), ret.x.data(), static_cast<int>(ret.x.size()));
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
    EcKeyPtr key = CreatePrivateEcKey(private_key);
    if (!key) {
        LOG_ERROR(HW, "Failed to construct private EC key");
        return {};
    }
    return GetPublicKeyFromEcKey(key.get());
}

std::pair<PrivateKey, PublicKey> GenerateKeyPair() {
    PrivateKey private_key{};
    EcGroupPtr group = CreateGroup();
    EcKeyPtr key(EC_KEY_new(), &EC_KEY_free);
    if (!group || !key || EC_KEY_set_group(key.get(), group.get()) != 1 ||
        EC_KEY_generate_key(key.get()) != 1) {
        LOG_ERROR(HW, "Failed to generate EC key pair");
        return {private_key, {}};
    }

    const BIGNUM* priv = EC_KEY_get0_private_key(key.get());
    if (!priv || BN_bn2binpad(priv, private_key.x.data(), static_cast<int>(private_key.x.size())) !=
                     static_cast<int>(private_key.x.size())) {
        LOG_ERROR(HW, "Failed to export generated private key");
        return {private_key, {}};
    }

    return std::make_pair(private_key, GetPublicKeyFromEcKey(key.get()));
}

Signature Sign(std::span<const u8> data, PrivateKey private_key) {
    Signature ret{};
    EcKeyPtr key = CreatePrivateEcKey(private_key);
    if (!key) {
        LOG_ERROR(HW, "Failed to create private key for signing");
        return ret;
    }

    EcSigPtr sig(ECDSA_do_sign(data.data(), static_cast<int>(data.size()), key.get()), &ECDSA_SIG_free);
    if (!sig) {
        LOG_ERROR(HW, "ECDSA_do_sign failed");
        return ret;
    }
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig.get(), &r, &s);
    if (!r || !s) {
        LOG_ERROR(HW, "ECDSA signature missing r/s components");
        return ret;
    }
    BN_bn2binpad(r, ret.r.data(), static_cast<int>(ret.r.size()));
    BN_bn2binpad(s, ret.s.data(), static_cast<int>(ret.s.size()));
    return ret;
}

bool Verify(std::span<const u8> data, Signature signature, PublicKey public_key) {
    EcKeyPtr key = CreatePublicEcKey(public_key);
    if (!key) {
        LOG_ERROR(HW, "Failed to construct public EC key");
        return false;
    }

    BnPtr r = CreateBigNum(signature.r);
    BnPtr s = CreateBigNum(signature.s);
    EcSigPtr sig(ECDSA_SIG_new(), &ECDSA_SIG_free);
    if (!r || !s || !sig || ECDSA_SIG_set0(sig.get(), r.release(), s.release()) != 1) {
        LOG_ERROR(HW, "Failed to create ECDSA signature object for verification");
        return false;
    }

    return ECDSA_do_verify(data.data(), static_cast<int>(data.size()), sig.get(), key.get()) == 1;
}

std::vector<u8> Agree(PrivateKey private_key, PublicKey others_public_key) {
    EcKeyPtr local_key = CreatePrivateEcKey(private_key);
    if (!local_key) {
        LOG_ERROR(HW, "Failed to construct private EC key for ECDH");
        return {};
    }
    const EC_GROUP* group = EC_KEY_get0_group(local_key.get());
    if (!group) {
        LOG_ERROR(HW, "Missing EC group for ECDH");
        return {};
    }
    EcPointPtr remote_point = CreatePoint(group, others_public_key);
    if (!remote_point) {
        LOG_ERROR(HW, "Invalid remote EC public key for ECDH");
        return {};
    }

    const int degree = EC_GROUP_get_degree(group);
    const std::size_t secret_len = static_cast<std::size_t>((degree + 7) / 8);
    std::vector<u8> agreement(secret_len);
    const int out_len =
        ECDH_compute_key(agreement.data(), static_cast<int>(agreement.size()), remote_point.get(),
                         local_key.get(), nullptr);
    if (out_len <= 0) {
        LOG_ERROR(HW, "ECDH agreement failed");
        return {};
    }
    agreement.resize(static_cast<std::size_t>(out_len));
    return agreement;
}

const PublicKey& GetRootPublicKey() {
    return root_public;
}

} // namespace HW::ECC
