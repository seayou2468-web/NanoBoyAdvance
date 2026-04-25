# Aurora3DS Crypto++ migration status (iOS SDK)

This document tracks remaining direct `CryptoPP` dependencies under `src/core/aurora3ds/src/core`.

## Current remaining direct Crypto++ include locations

(Generated from: `rg -n "#include <cryptopp|#include \"cryptopp|CryptoPP::" src/core/aurora3ds/src/core -S`)

### ECC / RSA (largest blocker)
- `src/core/aurora3ds/src/core/hw/ecc.h`
- `src/core/aurora3ds/src/core/hw/ecc.cpp`
- `src/core/aurora3ds/src/core/hw/rsa/rsa.cpp`
- `src/core/aurora3ds/src/core/file_sys/certificate.cpp`

Notes:
- ECC uses `sect233r1` / EC2N operations and ECDH/ECDSA APIs from Crypto++.
- RSA path relies on big integer modular exponentiation and PKCS#1 v1.5 SHA-256 sign/verify.
- These are the highest-risk pieces for full iOS SDK-only migration.

### AES / Hash paths still using Crypto++ directly
- `src/core/aurora3ds/src/core/hle/service/fs/fs_user.cpp` (CMAC path still Crypto++)

### AES-CCM / MD5 / HMAC / Base64 / protocol crypto
- `src/core/aurora3ds/src/core/hw/aes/ccm.cpp`
- `src/core/aurora3ds/src/core/hle/service/nwm/uds_data.cpp`
- `src/core/aurora3ds/src/core/hle/service/cecd/cecd.cpp`

### Compatibility wrappers (non-Apple fallback still points to Crypto++)
- `src/core/aurora3ds/src/core/common/crypto_util.h`
- `src/core/aurora3ds/src/core/common/aes_util.h`

## What has already been migrated to iOS SDK wrappers
- RNG/SHA utility abstraction in `common/crypto_util.h` (Apple path uses Security + CommonCrypto).
- AES utility abstraction in `common/aes_util.h` (Apple path uses CommonCrypto).
- Direct call-site migrations already done in:
  - `src/core/aurora3ds/src/core/hle/service/ssl/ssl_c.cpp`
  - `src/core/aurora3ds/src/core/hle/service/am/am.cpp`
  - `src/core/aurora3ds/src/core/file_sys/ticket.cpp`
  - `src/core/aurora3ds/src/core/hle/service/cfg/cfg.cpp`
  - `src/core/aurora3ds/src/core/hle/service/nfc/nfc_device.cpp`
  - `src/core/aurora3ds/src/core/hw/unique_data.cpp`
  - `src/core/aurora3ds/src/core/file_sys/title_metadata.cpp`
  - `src/core/aurora3ds/src/core/file_sys/cia_container.cpp`
  - `src/core/aurora3ds/src/core/file_sys/otp.cpp`
  - `src/core/aurora3ds/src/core/hle/service/ps/ps_ps.cpp`
  - `src/core/aurora3ds/src/core/hle/service/dlp/dlp_crypto.cpp`
  - `src/core/aurora3ds/src/core/hle/service/http/http_c.cpp`
  - `src/core/aurora3ds/src/core/hle/service/fs/fs_user.cpp` (SHA/CBC path)
  - `src/core/aurora3ds/src/core/hw/aes/key.cpp`
  - `src/core/aurora3ds/src/core/hle/service/dlp/dlp_base.cpp`
  - `src/core/aurora3ds/src/core/file_sys/romfs_reader.cpp`
  - `src/core/aurora3ds/src/core/file_sys/ncch_container.cpp`
  - `src/core/aurora3ds/src/core/hle/service/nwm/uds_beacon.cpp`
  - `src/core/aurora3ds/src/core/hle/service/nwm/nwm_uds.cpp`
  - `src/core/aurora3ds/src/core/hle/service/nfc/amiibo_crypto.cpp`
  - `src/core/aurora3ds/src/core/hle/service/nfc/amiibo_crypto.h`

## Recommended next migration order
1. Migrate direct AES/CTR/CBC users to `common/aes_util.h` (`fs_user.cpp` CMAC, `hw/aes/ccm.cpp`, protocol-specific crypto paths).
2. Migrate hash/random direct users to `common/crypto_util.h` where still present.
3. Replace CCM/HMAC/MD5/base64 protocol code paths with iOS SDK-compatible implementations.
4. Replace ECC/RSA code paths last (requires explicit treatment for `sect233r1` and big integer operations).
