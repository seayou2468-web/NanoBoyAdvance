#pragma once

#if defined(__x86_64__) || defined(_M_X64)
#define CITRA_ARCH_x86_64 1
#define CITRA_ARCH(x) CITRA_ARCH_##x
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CITRA_ARCH_arm64 1
#define CITRA_ARCH(x) CITRA_ARCH_##x
#else
#define CITRA_ARCH(x) 0
#endif
