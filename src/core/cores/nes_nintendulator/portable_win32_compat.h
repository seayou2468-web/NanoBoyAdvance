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
using DWORD = unsigned long;
using BOOL = int;
using UINT = unsigned int;
using HRESULT = int;
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
using LPBYTE = unsigned char*;

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

struct RECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
};

struct WAVEFORMATEX {
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
  WORD wBitsPerSample;
  WORD cbSize;
};

struct DSBUFFERDESC {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwBufferBytes;
  WAVEFORMATEX* lpwfxFormat;
};

struct IDirectSoundBuffer {
  HRESULT SetFormat(const WAVEFORMATEX*) { return 0; }
  HRESULT Play(DWORD, DWORD, DWORD) { return 0; }
  HRESULT Lock(DWORD, DWORD, LPVOID* p1, DWORD* b1, LPVOID*, DWORD*, DWORD) {
    static unsigned char dummy[4096] = {};
    *p1 = dummy;
    *b1 = sizeof(dummy);
    return 0;
  }
  HRESULT Unlock(LPVOID, DWORD, LPVOID, DWORD) { return 0; }
  HRESULT Stop() { return 0; }
  HRESULT GetCurrentPosition(DWORD* read, DWORD* write) {
    if (read) *read = 0;
    if (write) *write = 0;
    return 0;
  }
  HRESULT Release() { return 0; }
};

struct IDirectSound {
  HRESULT SetCooperativeLevel(HWND, DWORD) { return 0; }
  HRESULT CreateSoundBuffer(const DSBUFFERDESC*, IDirectSoundBuffer** out, LPVOID) {
    static IDirectSoundBuffer buffer{};
    *out = &buffer;
    return 0;
  }
  HRESULT Release() { return 0; }
};

using LPDIRECTSOUND = IDirectSound*;
using LPDIRECTSOUNDBUFFER = IDirectSoundBuffer*;

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
inline HWND GetDlgItem(HWND, int) { return nullptr; }
inline BOOL CheckDlgButton(HWND, int, UINT) { return TRUE; }
inline UINT IsDlgButtonChecked(HWND, int) { return 0; }
inline int DialogBox(HINSTANCE, const TCHAR*, HWND, INT_PTR (*)(HWND, UINT, WPARAM, LPARAM)) { return -1; }
inline UINT CheckMenuItem(HMENU, UINT, UINT) { return 0; }
inline BOOL CheckMenuRadioItem(HMENU, UINT, UINT, UINT, UINT) { return TRUE; }
inline BOOL EnableMenuItem(HMENU, UINT, UINT) { return TRUE; }
inline BOOL DrawMenuBar(HWND) { return TRUE; }
inline BOOL ShowWindow(HWND, int) { return TRUE; }
inline BOOL DestroyWindow(HWND) { return TRUE; }
inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return TRUE; }
inline HRESULT DirectSoundCreate(const void*, LPDIRECTSOUND* out, LPVOID) {
  static IDirectSound ds{};
  *out = &ds;
  return 0;
}
inline LONG RegSetValueEx(HKEY, const TCHAR*, DWORD, DWORD, const LPBYTE, DWORD) { return 0; }
inline LONG RegOpenKeyEx(HKEY, const TCHAR*, DWORD, DWORD, HKEY* out) {
  if (out) *out = nullptr;
  return 0;
}
inline LONG RegCreateKeyEx(HKEY, const TCHAR*, DWORD, TCHAR*, DWORD, DWORD, LPVOID, HKEY* out, DWORD*) {
  if (out) *out = nullptr;
  return 0;
}
inline LONG RegCloseKey(HKEY) { return 0; }
inline LONG RegQueryValueEx(HKEY, const TCHAR*, DWORD*, DWORD*, LPBYTE, DWORD* size) {
  if (size != nullptr) {
    *size = sizeof(DWORD);
  }
  return 0;
}
inline void Sleep(DWORD) {}
inline DWORD GetFileAttributes(const TCHAR*) { return 0; }
inline BOOL CreateDirectory(const TCHAR*, LPVOID) { return TRUE; }
inline BOOL GetWindowRect(HWND, RECT* r) {
  if (r) {
    r->left = 0;
    r->top = 0;
    r->right = 0;
    r->bottom = 0;
  }
  return TRUE;
}
inline BOOL GetClientRect(HWND, RECT* r) { return GetWindowRect(nullptr, r); }
inline BOOL RemoveDirectory(const TCHAR*) { return TRUE; }
inline BOOL MoveFile(const TCHAR*, const TCHAR*) { return TRUE; }
inline BOOL PathAppend(TCHAR* base, const TCHAR* suffix) {
  std::strcat(base, "/");
  std::strcat(base, suffix);
  return TRUE;
}
template <typename ThreadEntry>
inline HANDLE CreateThread(LPVOID, std::size_t, ThreadEntry, LPVOID, DWORD, DWORD* id) {
  if (id) *id = 0;
  return nullptr;
}
inline BOOL CloseHandle(HANDLE) { return TRUE; }
inline HRESULT SHGetFolderPath(HWND, int, HANDLE, DWORD, TCHAR* out) {
  std::strcpy(out, ".");
  return 0;
}
inline int GetSystemMetrics(int) { return 0; }

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
#define INVALID_FILE_ATTRIBUTES ((DWORD)(-1))
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define CSIDL_PERSONAL 0x0005
#define FAILED(x) ((x) != 0)
#define SUCCEEDED(x) ((x) == 0)
#define LOWORD(l) ((unsigned short)((l) & 0xFFFF))
#define HIWORD(l) ((unsigned short)(((l) >> 16) & 0xFFFF))
#define MAKELONG(a,b) ((long)(((unsigned short)(a)) | ((unsigned long)((unsigned short)(b))) << 16))
#define GWLP_USERDATA 0
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_VSCROLL 0x0115
#define LB_INSERTSTRING 0x0181
#define LB_GETCURSEL 0x0188
#define LB_ERR (-1)
#define TBM_SETRANGE 0x0400
#define TBM_SETTICFREQ 0x0401
#define TBM_SETPOS 0x0405
#define TBM_GETPOS 0x0400
#define LBN_DBLCLK 2
#define BST_UNCHECKED 0
#define BST_CHECKED 1
#define IDCANCEL 2
#define IDOK 1
#define MF_CHECKED 0x0008
#define MF_UNCHECKED 0x0000
#define MF_GRAYED 0x0001
#define MF_ENABLED 0x0000
#define MF_BYCOMMAND 0x0000
#define SW_SHOW 5
#define SW_HIDE 0
#define HWND_TOP ((HWND)0)
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define DSBPLAY_LOOPING 1
#define DSBLOCK_ENTIREBUFFER 1
#define DSBCAPS_PRIMARYBUFFER 0x1
#define DSBCAPS_GLOBALFOCUS 0x2
#define DSBCAPS_LOCSOFTWARE 0x4
#define DSBCAPS_GETCURRENTPOSITION2 0x8
#define DSSCL_PRIORITY 1
#define REG_DWORD 4
#define REG_SZ 1
#define KEY_ALL_ACCESS 0xF003F
#define HKEY_CURRENT_USER ((HKEY)1)
#define REG_OPTION_NON_VOLATILE 0
#define WAVE_FORMAT_PCM 1
#define SM_XVIRTUALSCREEN 76
#define SM_YVIRTUALSCREEN 77
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
inline const int DSDEVID_DefaultPlayback = 0;

struct POINT {
  LONG x;
  LONG y;
};

using LPDIRECTDRAW7 = void*;
