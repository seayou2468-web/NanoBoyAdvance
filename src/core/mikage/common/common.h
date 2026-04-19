// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _COMMON_H_
#define _COMMON_H_

// DO NOT EVER INCLUDE <windows.h> directly _or indirectly_ from this file
// since it slows down the build a lot.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wundefined-inline"
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wdangling-else"
#pragma clang diagnostic ignored "-Wparentheses-equality"
#endif

// Force enable logging in the right modes. For some reason, something had changed
// so that debugfast no longer logged.
#if defined(_DEBUG) || defined(DEBUGFAST)
#undef LOGGING
#define LOGGING 1
#endif

#define STACKALIGN

#if __cplusplus >= 201103L || defined(_MSC_VER) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#define HAVE_CXX11_SYNTAX 1
#endif

#if HAVE_CXX11_SYNTAX
// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
    NonCopyable() {}
    NonCopyable(const NonCopyable&&) {}
    void operator=(const NonCopyable&&) {}
private:
    NonCopyable(NonCopyable&);
    NonCopyable& operator=(NonCopyable& other);
};
#endif

#include "log.h"
#include "common_types.h"
#include "msg_handler.h"
#include "common_funcs.h"
#include "common_paths.h"
#include "platform.h"

#ifdef __APPLE__
// The Darwin ABI requires that stack frames be aligned to 16-byte boundaries.
// This is only needed on i386 gcc - x86_64 already aligns to 16 bytes.
#if defined __i386__ && defined __GNUC__
#undef STACKALIGN
#define STACKALIGN __attribute__((__force_align_arg_pointer__))
#endif

#endif

// iOS-compatible alignment and definitions
#include <limits.h>
#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#define __forceinline inline __attribute__((always_inline))
#define MEMORY_ALIGNED16(x) __attribute__((aligned(16))) x
#define MEMORY_ALIGNED32(x) __attribute__((aligned(32))) x
#define MEMORY_ALIGNED64(x) __attribute__((aligned(64))) x
#define MEMORY_ALIGNED128(x) __attribute__((aligned(128))) x
#define MEMORY_ALIGNED16_DECL(x) __attribute__((aligned(16))) x
#define MEMORY_ALIGNED64_DECL(x) __attribute__((aligned(64))) x

// iOS standard functions
#define __strdup strdup
#define __getcwd getcwd
#define __chdir chdir

// Memory leak checks (stub)
#define CHECK_HEAP_INTEGRITY()

// Dummy macro for marking translatable strings that can not be immediately translated.
// wxWidgets does not have a true dummy macro for this.
#define _trans(a) a

#if defined(__aarch64__) || defined(__arm64__)
#define IOS_ARM64 1
#else
#define IOS_ARM64 0
#endif
#define IOS_X86_REMOVED 0

#define _M_SSE 0x0

// Host communication.
enum HOST_COMM
{
    // Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
    WM_USER_STOP = 10,
    WM_USER_CREATE,
    WM_USER_SETCURSOR,
};

// Used for notification on emulation state
enum EMUSTATE_CHANGE
{
    EMUSTATE_CHANGE_PLAY = 1,
    EMUSTATE_CHANGE_PAUSE,
    EMUSTATE_CHANGE_STOP
};

// Byte-swap helpers (portable fallback path).
inline unsigned short bswap16(unsigned short x) { return (x << 8) | (x >> 8); }
inline unsigned int bswap32(unsigned int x) {
    return (x >> 24) | ((x & 0xFF0000) >> 8) | ((x & 0xFF00) << 8) | (x << 24);
}
inline unsigned long long bswap64(unsigned long long x) {
    return (static_cast<unsigned long long>(bswap32(static_cast<unsigned int>(x))) << 32) |
           bswap32(static_cast<unsigned int>(x >> 32));
}

inline float bswapf(float f) {
    union {
        float f;
        unsigned int u32;
    } dat1, dat2;

    dat1.f = f;
    dat2.u32 = bswap32(dat1.u32);

    return dat2.f;
}

inline double bswapd(double f) {
    union  {
        double f;
        unsigned long long u64;
    } dat1, dat2;

    dat1.f = f;
    dat2.u64 = bswap64(dat1.u64);

    return dat2.f;
}

#include "swap.h"

#endif // _COMMON_H_
