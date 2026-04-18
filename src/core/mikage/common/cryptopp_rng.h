// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cryptopp/rng.h>
#include "common/secure_random.h"

namespace Common {

class CryptoPPRandom final : public CryptoPP::RandomNumberGenerator {
public:
    ~CryptoPPRandom() override = default;

    std::string AlgorithmName() const override {
        return "Common::CryptoPPRandom";
    }

    void GenerateBlock(byte* output, size_t size) override {
        FillSecureRandom(std::span<std::uint8_t>(output, size));
    }
};

} // namespace Common
