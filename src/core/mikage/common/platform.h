/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    platform.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Platform detection macros for portable compilation
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#ifndef COMMON_PLATFORM_H_
#define COMMON_PLATFORM_H_

#include "common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform definitions

/// Enumeration for defining the supported platforms
#define PLATFORM_NULL 0
#define PLATFORM_IOS 1

////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform detection

#ifndef EMU_PLATFORM

#if defined(__APPLE__) || defined(__APPLE_CC__)
#include <TargetConditionals.h>
#define EMU_PLATFORM PLATFORM_IOS

#else
#error "Mikage common now targets iOS SDK only."
#endif

#endif

#define EMU_ARCHITECTURE_ARM64

////////////////////////////////////////////////////////////////////////////////////////////////////
// Compiler-Specific Definitions

#define EMU_FASTCALL __attribute__((fastcall))
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

#define GCC_VERSION_AVAILABLE(major, minor) (defined(__GNUC__) &&  (__GNUC__ > (major) || \
    (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))

#endif // COMMON_PLATFORM_H_
