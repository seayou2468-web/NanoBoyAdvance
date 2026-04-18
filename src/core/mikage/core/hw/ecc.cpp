// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include "common/assert.h"
#include "common/common_paths.h"
#include "common/crypto/cryptopp_ecc_compat.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/secure_random.h"
#include "common/string_util.h"
#include "core/hw/aes/key.h"
#include "core/hw/ecc.h"

namespace HW::ECC {

PublicKey root_public;

namespace {

using CryptoPPInteger = CryptoPP::Integer;
using CryptoPPPoint = CryptoPP::EC2N::Point;
using CryptoPPECCPrivateKey = CryptoPP::ECDSA<CryptoPP::EC2N, CryptoPP::SHA256>::PrivateKey;
using CryptoPPECCPublicKey = CryptoPP::ECDSA<CryptoPP::EC2N, CryptoPP::SHA256>::PublicKey;

class CryptoPPRandom final : public CryptoPP::RandomNumberGenerator {
public:
    ~CryptoPPRandom() override = default;

    std::string AlgorithmName() const override {
        return "HW::ECC::CryptoPPRandom";
    }

    void GenerateBlock(byte* output, size_t size) override {
        Common::FillSecureRandom(std::span<std::uint8_t>(output, size));
    }
};

CryptoPPInteger AsCryptoPPInteger(const PrivateKey& private_key) {
    return CryptoPP::Integer(private_key.x.data(), private_key.x.size(), CryptoPP::Integer::UNSIGNED,
                             CryptoPP::BIG_ENDIAN_ORDER);
}

CryptoPPECCPrivateKey AsCryptoPPPrivateKey(const PrivateKey& private_key) {
    CryptoPPECCPrivateKey private_key_cpp;
    CryptoPPRandom prng;

    private_key_cpp.Initialize(CryptoPP::ASN1::sect233r1(), AsCryptoPPInteger(private_key));
    if (!private_key_cpp.Validate(prng, 3)) {
        LOG_ERROR(HW, "Failed to verify ECC private key");
    }

    return private_key_cpp;
}

CryptoPPPoint AsCryptoPPPoint(const PublicKey& public_key) {
    return CryptoPP::EC2N::Point(CryptoPP::PolynomialMod2(public_key.x.data(), public_key.x.size()),
                                 CryptoPP::PolynomialMod2(public_key.y.data(), public_key.y.size()));
}

CryptoPPECCPublicKey AsCryptoPPPublicKey(const PublicKey& public_key) {
    CryptoPPECCPublicKey public_key_cpp;

    public_key_cpp.Initialize(CryptoPP::ASN1::sect233r1(), AsCryptoPPPoint(public_key));
    return public_key_cpp;
}

PublicKey MakePublicKeyFromCrypto(const CryptoPPECCPrivateKey& private_key_cpp) {
    CryptoPPECCPublicKey public_key_cpp;
    PublicKey public_key;

    private_key_cpp.MakePublicKey(public_key_cpp);

    public_key_cpp.GetPublicElement().x.Encode(public_key.x.data(), public_key.x.size());
    public_key_cpp.GetPublicElement().y.Encode(public_key.y.data(), public_key.y.size());

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
    CryptoPPECCPrivateKey private_key;
    CryptoPPInteger privk_x(private_key_x.data(), private_key_x.size(), CryptoPP::Integer::UNSIGNED,
                            CryptoPP::BIG_ENDIAN_ORDER);

    // The ECC library Nintendo used to generate private keys does not limit the private key
    // size to be inside the subgroup order. To fix this, we do a modulo operation with the
    // subgroup order, otherwise CryptoPP will fail to use the key.
    if (fix_up) {
        CryptoPP::DL_GroupParameters_EC<CryptoPP::EC2N> params(CryptoPP::ASN1::sect233r1());
        privk_x = privk_x % params.GetSubgroupOrder();
    }
    private_key.Initialize(CryptoPP::ASN1::sect233r1(), privk_x);

    PrivateKey ret;
    private_key.GetPrivateExponent().Encode(ret.x.data(), ret.x.size());
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
    return MakePublicKeyFromCrypto(AsCryptoPPPrivateKey(private_key));
}

std::pair<PrivateKey, PublicKey> GenerateKeyPair() {
    CryptoPPECCPrivateKey private_key_cpp;
    PrivateKey private_key;

    CryptoPPRandom prng;

    private_key_cpp.Initialize(prng, CryptoPP::ASN1::sect233r1());
    private_key_cpp.GetPrivateExponent().Encode(private_key.x.data(), private_key.x.size());

    return std::make_pair(private_key, MakePublicKeyFromCrypto(private_key_cpp));
}

Signature Sign(std::span<const u8> data, PrivateKey private_key) {
    CryptoPP::ECDSA<CryptoPP::EC2N, CryptoPP::SHA256>::Signer signer(
        AsCryptoPPPrivateKey(private_key));
    CryptoPPRandom prng;

    Signature ret;

    signer.SignMessage(prng, data.data(), data.size(), ret.rs.data());
    return ret;
}

bool Verify(std::span<const u8> data, Signature signature, PublicKey public_key) {
    CryptoPP::ECDSA<CryptoPP::EC2N, CryptoPP::SHA256>::Verifier verifier(
        AsCryptoPPPublicKey(public_key));

    return verifier.VerifyMessage(data.data(), data.size(), signature.rs.data(),
                                  signature.rs.size());
}

std::vector<u8> Agree(PrivateKey private_key, PublicKey others_public_key) {
    CryptoPP::ECDH<CryptoPP::EC2N, CryptoPP::NoCofactorMultiplication>::Domain domain(
        CryptoPP::ASN1::sect233r1());
    CryptoPP::DL_GroupParameters_EC<CryptoPP::EC2N> params(CryptoPP::ASN1::sect233r1());
    std::vector<u8> agreement(domain.AgreedValueLength());

    std::vector<u8> private_encoded(domain.PrivateKeyLength());
    AsCryptoPPInteger(private_key).Encode(private_encoded.data(), private_encoded.size());

    std::vector<u8> others_public_encoded(params.GetEncodedElementSize(true));
    params.EncodeElement(true, AsCryptoPPPoint(others_public_key), others_public_encoded.data());

    if (!domain.Agree(agreement.data(), private_encoded.data(), others_public_encoded.data())) {
        LOG_ERROR(HW, "ECDH agreement failed");
    }

    return agreement;
}

const PublicKey& GetRootPublicKey() {
    return root_public;
}

} // namespace HW::ECC
