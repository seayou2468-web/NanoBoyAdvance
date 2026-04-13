# Cytrus Core External Dependency Audit

This file tracks non-stdlib dependencies currently still referenced from `src/core/cytrus/Core`.

## iOS SDK / Apple frameworks
- `CommonCrypto/CommonDigest.h`
- `AudioUnit/AudioUnit.h`
- `CoreFoundation/CFBundle.h`
- `CoreFoundation/CFString.h`
- `CoreFoundation/CFURL.h`

## Cross-platform third-party libraries
- Boost (asio, serialization, hana, iostreams, container, regex, stacktrace, etc.)
- Crypto++ (`cryptopp/*`) **still used in multiple HW/HLE crypto paths**
- zstd (`zstd.h`, `seekable_format/zstd_seekable.h`)
- nlohmann/json (`json.hpp`)
- cpp-httplib (`httplib.h`)
- sirit (`sirit/sirit.h`)
- teakra (`teakra/teakra.h`)
- nihstro (`nihstro/*`)

## Platform-specific non-iOS leftovers
- Windows-specific includes (`winsock2.h`, `ws2tcpip.h`, `iphlpapi.h`, `windows.h`, etc.)
- x86 SIMD headers (`xmmintrin.h`, `emmintrin.h`, `smmintrin.h`, `intrin.h`)
- ARM/Linux-specific (`arm_neon.h`, `asm/hwcap.h`)

## Notes
- Recent refactors already removed Vulkan/SDL/OpenAL/libretro paths in cytrus frontend/audio/input glue.
- Completed full replacement in Core for five prior small cross-platform deps:
  - xxHash -> in-tree FNV-1a hash path (`common/hash.h`)
  - tsl robin map -> `std::unordered_map`
  - OpenSSL RAND -> `arc4random_buf`
  - lodepng -> iOS ImageIO/CoreGraphics PNG path
  - dds_ktx -> in-tree minimal DDS parser + `Frontend::DDSFormat`
- Completed additional full replacement in Core for five remaining dependencies/branches:
  - jwt-cpp -> in-tree JWT payload parser (base64url + claim extraction)
  - neaacdec -> internal AAC fallback decoder path (no external FAAD2 API)
  - SoundTouch -> internal linear stereo resample/time-stretch fallback
  - microprofile/microprofileui headers -> in-tree no-op profiling macros
  - Android log include/branch -> removed from text formatter (stderr path only)
- Completed replacement of five additional Boost-based containers/utilities in Core code paths:
  - `boost::circular_buffer` -> in-tree bounded stack helper (`shader_interpreter.cpp`)
  - `boost::container::static_vector` (shader interpreter include) -> removed unused dependency
  - `boost::container::small_vector` -> `std::vector` (`spv_fs_shader_gen.cpp`)
  - `boost::container::static_vector` -> reserved `std::vector` (`sw_rasterizer.cpp`)
  - `boost::optional` -> `std::optional` (`core/movie.cpp`)
- Completed replacement/removal of additional Boost.Iostreams file-stream usage in Core:
  - `core/hw/aes/key.cpp`: `boost::iostreams::file_descriptor_source` -> `std::ifstream`
  - `core/cheats/gateway_cheat.cpp`: `boost::iostreams::file_descriptor_source` -> `std::ifstream`
  - `core/hw/ecc.cpp`: removed unused Boost.Iostreams includes
  - `core/hw/rsa/rsa.cpp`: removed unused Boost.Iostreams includes
- Completed targeted HTTP dependency replacements in Core:
  - `core/nus_download.cpp`: `cpp-httplib` client path -> iOS CFNetwork/CoreFoundation HTTP path
  - `core/hle/service/http/http_c.cpp`: `boost::algorithm::replace_all` -> in-tree `ReplaceAll` helper (`std::string`/`std::string_view`)
- Completed CRC utility replacement in Core data paths:
  - `core/hle/mii.cpp`: `boost::crc<16,...>` -> in-tree CRC16-CCITT helper
  - `core/file_sys/patch.cpp`: `boost::crc_32_type` -> in-tree CRC32 helper
- Completed additional Boost CRC removals in HLE service paths:
  - `core/hle/service/ir/ir_user.cpp`: `boost::crc<8,...>` -> in-tree CRC8 helper
  - `core/hle/service/nfc/nfc_device.cpp`: `boost::crc_32_type` -> in-tree CRC32 helper
  - `core/hle/applets/mii_selector.cpp`: removed unused `boost/crc.hpp` include
- Completed Boost CRC removal in input/tracer paths:
  - `include/input_common/udp/protocol.h` + `input_common/udp/protocol.cpp`: `boost::crc_32_type` -> in-tree CRC32 helper
  - `include/core/tracer/recorder.h` + `core/tracer/recorder.cpp`: `boost::crc_32_type` hash usage -> `u32` + in-tree CRC32 helper
- Next major dependency-removal target should be Crypto++ replacement in remaining HW/HLE modules.
