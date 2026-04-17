// iOS Compatibility Layer for Architecture Configuration
// Platform detection and architecture-specific settings
#pragma once

#include <cstdint>

// CPU architecture detection
#ifdef __ARM_ARCH
#define CPU_ARCH_ARM 1
#else
#define CPU_ARCH_ARM 0
#endif

#ifdef __aarch64__
#define CPU_ARCH_ARM64 1
#else
#define CPU_ARCH_ARM64 0
#endif

#ifdef __x86_64__
#define CPU_ARCH_X64 1
#else
#define CPU_ARCH_X64 0
#endif

// Byte order detection
#ifdef __LITTLE_ENDIAN__
#define CPU_LITTLE_ENDIAN 1
#define CPU_BIG_ENDIAN 0
#elif defined(__BIG_ENDIAN__)
#define CPU_LITTLE_ENDIAN 0
#define CPU_BIG_ENDIAN 1
#else
// Default to little endian for most platforms
#define CPU_LITTLE_ENDIAN 1
#define CPU_BIG_ENDIAN 0
#endif

// Memory alignment
#define ALIGNMENT_16 __attribute__((aligned(16)))
#define ALIGNMENT_32 __attribute__((aligned(32)))
#define ALIGNMENT_64 __attribute__((aligned(64)))
