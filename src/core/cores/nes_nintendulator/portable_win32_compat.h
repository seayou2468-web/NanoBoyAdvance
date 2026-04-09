#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <strings.h>

#ifndef __cdecl
#define __cdecl
#endif

#ifndef __fastcall
#define __fastcall
#endif

#ifndef __forceinline
#define __forceinline inline
#endif

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using BOOL = int;
using UINT = unsigned int;
using LONG = long;
using LPARAM = long;
using WPARAM = unsigned long;
using LPVOID = void*;
using HANDLE = void*;
using HWND = void*;
using HMENU = void*;
using HACCEL = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using HKEY = void*;
using FARPROC = void*;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

using TCHAR = char;

#ifndef _T
#define _T(x) x
#endif

#ifndef MB_OK
#define MB_OK 0
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR 0
#endif

inline int MessageBox(HWND, const TCHAR*, const TCHAR*, unsigned int) { return 0; }
inline void ZeroMemory(void* p, std::size_t n) { std::memset(p, 0, n); }

#define _tfopen std::fopen
#define _tcslen std::strlen
#define _tcsicmp strcasecmp
#define _stprintf std::snprintf
#define _tcsdup strdup

struct POINT {
  LONG x;
  LONG y;
};

using LPDIRECTDRAW7 = void*;
