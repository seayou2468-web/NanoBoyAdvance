// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "../../../../common/archives.h"
#include "../../../../common/commoncrypto_aes.h"
#include "../../../../common/logging/log.h"
#include "../../../core.h"
#include "../../ipc_helpers.h"
#include "dlp_base.h"
#include "../ssl/ssl_c.h"
#include "../../../hw/aes/arithmetic128.h"
#include "../../../hw/aes/key.h"

namespace Service::DLP {

void DLP_Base::DLPEncryptCTR(void* _out, size_t size, const u8* iv_ctr) {
    auto out = reinterpret_cast<u8*>(_out);
    memset(out, 0, size);

    HW::AES::SelectDlpNfcKeyYIndex(HW::AES::DlpNfcKeyY::Dlp);
    HW::AES::AESKey key = HW::AES::GetNormalKey(HW::AES::DLPNFCDataKey);

    Common::Crypto::AESBlock ctr{};
    std::copy_n(iv_ctr, ctr.size(), ctr.begin());
    if (!Common::Crypto::AESCTRTransform(std::span<const u8>(out, size), std::span<u8>(out, size),
                                         key, ctr)) {
        LOG_ERROR(Service_DLP, "DLPEncryptCTR failed");
    }
}

} // namespace Service::DLP
