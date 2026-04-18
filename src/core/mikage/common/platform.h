/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * iOS-only platform detection shim for Mikage core.
 */

#ifndef COMMON_PLATFORM_H_
#define COMMON_PLATFORM_H_

#include "common_types.h"

#define PLATFORM_NULL 0
#define PLATFORM_IOS 1

// iOS-only build target.
#ifndef EMU_PLATFORM
#define EMU_PLATFORM PLATFORM_IOS
#endif

#define EMU_ARCHITECTURE_ARM64

#define EMU_FASTCALL
#define __stdcall
#define __cdecl

#ifndef LONG
#define LONG long
#endif
#define BOOL bool
#define DWORD u32

#include <limits.h>
#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#include <strings.h>
#define stricmp(str1, str2) strcasecmp(str1, str2)
#define _stricmp(str1, str2) strcasecmp(str1, str2)
#define _snprintf snprintf
#define _getcwd getcwd
#define _tzset tzset

typedef void EXCEPTION_POINTERS;

#define GCC_VERSION_AVAILABLE(major, minor) (defined(__GNUC__) && (__GNUC__ > (major) || \
    (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))

#endif // COMMON_PLATFORM_H_
