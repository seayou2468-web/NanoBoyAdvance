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
- fmt (`fmt/core.h`, `fmt/format.h`, `fmt/ranges.h`, ...)
- Crypto++ (`cryptopp/*`) **still used in multiple HW/HLE crypto paths**
- FFmpeg (`libav*`, `libswresample`)
- zstd (`zstd.h`, `seekable_format/zstd_seekable.h`)
- nlohmann/json (`json.hpp`)
- cpp-httplib (`httplib.h`)
- dynarmic (`dynarmic/interface/*`)
- oaknut (`oaknut/*`)
- xbyak (`xbyak/*`)
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
- Next major dependency-removal target should be Crypto++ replacement in remaining HW/HLE modules.
