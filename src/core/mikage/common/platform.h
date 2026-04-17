#ifndef COMMON_PLATFORM_H_
#define COMMON_PLATFORM_H_

#include "common_types.h"

#define PLATFORM_NULL 0
#define PLATFORM_IOS 1
#define PLATFORM_LINUX 2

#ifndef EMU_PLATFORM
#if defined(__APPLE__)
#include <TargetConditionals.h>
#define EMU_PLATFORM PLATFORM_IOS
#elif defined(__linux__)
#define EMU_PLATFORM PLATFORM_LINUX
#else
#define EMU_PLATFORM PLATFORM_NULL
#endif
#endif

#define EMU_ARCHITECTURE_ARM64

#define EMU_FASTCALL
#define __stdcall
#define __cdecl

#ifndef LONG
#define LONG long
#endif
#define BOOL bool
#ifndef DWORD
#define DWORD u32
#endif

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

#endif // COMMON_PLATFORM_H_
