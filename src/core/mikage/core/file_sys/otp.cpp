// Copyright 2024 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <CommonCrypto/CommonDigest.h>
#include "../../common/commoncrypto_aes.h"
#include "../../common/file_util.h"
#include "../../common/logging/log.h"
#include "otp.h"
#include "../loader/loader.h"

namespace FileSys {
Loader::ResultStatus OTP::Load(const std::string& file_path, std::span<const u8> key,
                               std::span<const u8> iv) {
    FileUtil::IOFile file(file_path, "rb");
    if (!file.IsOpen())
        return Loader::ResultStatus::ErrorNotFound;

    if (file.GetSize() != sizeof(OTPBin)) {
        LOG_ERROR(HW_AES, "Invalid OTP size");
        return Loader::ResultStatus::Error;
    }

    OTPBin temp_otp;
    if (file.ReadBytes(&temp_otp, sizeof(OTPBin)) != sizeof(OTPBin)) {
        return Loader::ResultStatus::Error;
    }

    // OTP is probably encrypted, decrypt it.
    if (temp_otp.body.magic != otp_magic) {
        Common::Crypto::AESBlock aes_iv{};
        std::copy_n(iv.begin(), std::min(iv.size(), aes_iv.size()), aes_iv.begin());
        if (!Common::Crypto::AESCBCDecrypt(
                std::span<const u8>(reinterpret_cast<const u8*>(&temp_otp), sizeof(temp_otp)),
                std::span<u8>(reinterpret_cast<u8*>(&temp_otp), sizeof(temp_otp)), key, aes_iv)) {
            LOG_ERROR(HW_AES, "OTP decrypt failed");
            return Loader::ResultStatus::Error;
        }

        if (temp_otp.body.magic != otp_magic) {
            LOG_ERROR(HW_AES, "OTP failed to decrypt (or uses dev keys)");
            return Loader::ResultStatus::Error;
        }
    }

    // Verify OTP hash
    std::array<u8, CC_SHA256_DIGEST_LENGTH> digest;
    CC_SHA256(reinterpret_cast<const u8*>(&temp_otp.body),
              static_cast<CC_LONG>(sizeof(temp_otp.body)), digest.data());
    if (temp_otp.hash != digest) {
        LOG_ERROR(HW_AES, "OTP is corrupted");
        return Loader::ResultStatus::Error;
    }

    otp = temp_otp;
    return Loader::ResultStatus::Success;
}
} // namespace FileSys
