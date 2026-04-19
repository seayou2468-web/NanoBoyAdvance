#pragma once

#include <cstdio>
#include <string>

void etfprintf(FILE* file, const char* format, ...);
void etfprint(FILE* file, const std::string& text);

bool InitSymInfo(const char* initial_symbol_path);
bool UninitSymInfo();
void StackTrace();

#define EXTENDEDTRACEINITIALIZE(IniSymbolPath) ((void)InitSymInfo(IniSymbolPath))
#define EXTENDEDTRACEUNINITIALIZE() ((void)UninitSymInfo())
#define STACKTRACE(file) ((void)0)
#define STACKTRACE2(file, eip, esp, ebp) ((void)0)
