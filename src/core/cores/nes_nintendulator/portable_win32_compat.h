#pragma once

#include <cstddef>
#include <cstdarg>
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

#ifndef CALLBACK
#define CALLBACK
#endif

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using BOOL = int;
using UINT = unsigned int;
using LONG = long;
using LPARAM = long;
using WPARAM = unsigned long;
using INT_PTR = long;
using LPVOID = void*;
using HANDLE = void*;
using HWND = void*;
using HMENU = void*;
using HACCEL = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using HKEY = void*;
using FARPROC = void*;
using DWORD_PTR = std::uintptr_t;
using LONG_PTR = long;

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
inline int nintendulator_stprintf(char* out, const char* fmt, ...) {
  std::va_list ap;
  va_start(ap, fmt);
  const int rc = std::vsnprintf(out, 4096, fmt, ap);
  va_end(ap);
  return rc;
}
inline int nintendulator_vstprintf(char* out, const char* fmt, std::va_list ap) {
  return std::vsnprintf(out, 4096, fmt, ap);
}

struct WIN32_FIND_DATA {
  DWORD dwFileAttributes;
  TCHAR cFileName[MAX_PATH];
};

inline HANDLE FindFirstFile(const TCHAR*, WIN32_FIND_DATA*) { return nullptr; }
inline BOOL FindNextFile(HANDLE, WIN32_FIND_DATA*) { return FALSE; }
inline BOOL FindClose(HANDLE) { return TRUE; }
inline HMODULE LoadLibrary(const TCHAR*) { return nullptr; }
inline FARPROC GetProcAddress(HMODULE, const char*) { return nullptr; }
inline BOOL FreeLibrary(HMODULE) { return TRUE; }
inline int DialogBoxParam(HINSTANCE, const TCHAR*, HWND, INT_PTR (*)(HWND, UINT, WPARAM, LPARAM), LPARAM) {
  return -1;
}
inline LONG_PTR GetWindowLongPtr(HWND, int) { return 0; }
inline LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR) { return 0; }
inline long SendDlgItemMessage(HWND, int, unsigned int, WPARAM, LPARAM) { return 0; }
inline BOOL EndDialog(HWND, INT_PTR) { return TRUE; }
inline BOOL SetDlgItemText(HWND, int, const TCHAR*) { return TRUE; }

#define _tfopen std::fopen
#define _tcslen std::strlen
#define _tcsicmp strcasecmp
#define _stprintf nintendulator_stprintf
#define _vstprintf nintendulator_vstprintf
#define _tcsdup strdup
#define _tcscpy std::strcpy
#define _tcscat std::strcat
#define MAKEINTRESOURCE(i) ("")
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))
#define LOWORD(l) ((unsigned short)((l) & 0xFFFF))
#define HIWORD(l) ((unsigned short)(((l) >> 16) & 0xFFFF))
#define GWLP_USERDATA 0
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define LB_INSERTSTRING 0x0181
#define LB_GETCURSEL 0x0188
#define LB_ERR (-1)
#define LBN_DBLCLK 2
#define IDCANCEL 2
#define IDOK 1

struct POINT {
  LONG x;
  LONG y;
};

using LPDIRECTDRAW7 = void*;
