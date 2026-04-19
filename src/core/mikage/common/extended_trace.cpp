// --------------------------------------------------------------------------------------
// iOS-compatible stub for extended trace functionality
// --------------------------------------------------------------------------------------

#include <cstdarg>
#include <cstdio>

#include "extended_trace.h"

void etfprintf(FILE* file, const char* format, ...) {
    if (!file) {
        return;
    }

    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
}

void etfprint(FILE* file, const std::string& text) {
    if (!file) {
        return;
    }
    std::fprintf(file, "%s", text.c_str());
}

bool InitSymInfo(const char*) {
    return false;
}

bool UninitSymInfo() {
    return true;
}

void StackTrace() {}
