// -----------------------------------------------------------------------------------------
//
// iOS-compatible stub for extended trace functionality
// Original Windows-specific implementation removed
//
// -----------------------------------------------------------------------------------------

#ifndef _EXTENDEDTRACE_H_INCLUDED_
#define _EXTENDEDTRACE_H_INCLUDED_

#include <string>
#include <cstdio>

// iOS-compatible no-op macros for stack trace
#define EXTENDEDTRACEINITIALIZE(IniSymbolPath)      ((void)0)
#define EXTENDEDTRACEUNINITIALIZE()                 ((void)0)
#define STACKTRACE(file)                            ((void)0)
#define STACKTRACE2(file, eip, esp, ebp)            ((void)0)

#endif    // WIN32

#endif    // _EXTENDEDTRACE_H_INCLUDED_
