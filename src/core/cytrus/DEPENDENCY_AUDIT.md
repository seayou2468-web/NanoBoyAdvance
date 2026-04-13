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
- xxHash (`xxhash.h`)
- nlohmann/json (`json.hpp`)
- jwt-cpp (`jwt/jwt.hpp`)
- cpp-httplib (`httplib.h`)
- lodepng (`lodepng.h`)
- dynarmic (`dynarmic/interface/*`)
- oaknut (`oaknut/*`)
- xbyak (`xbyak/*`)
- sirit (`sirit/sirit.h`)
- teakra (`teakra/teakra.h`)
- tsl robin map (`tsl/robin_map.h`)
- nihstro (`nihstro/*`)
- dds_ktx (`dds_ktx.h`)
- neaacdec (`neaacdec.h`)
- microprofile (`microprofile.h`, `microprofileui.h`)
- openssl rand (`openssl/rand.h`)
- SoundTouch (`SoundTouch.h`)

## Platform-specific non-iOS leftovers
- Android-specific includes (`android/log.h`)
- Windows-specific includes (`winsock2.h`, `ws2tcpip.h`, `iphlpapi.h`, `windows.h`, etc.)
- x86 SIMD headers (`xmmintrin.h`, `emmintrin.h`, `smmintrin.h`, `intrin.h`)
- ARM/Linux-specific (`arm_neon.h`, `asm/hwcap.h`)

## Notes
- Recent refactors already removed Vulkan/SDL/OpenAL/libretro paths in cytrus frontend/audio/input glue.
- Next major dependency-removal target should be Crypto++ replacement in remaining HW/HLE modules.
