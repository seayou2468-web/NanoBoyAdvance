// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>

#include "common.h"
#include "console_listener.h"

#ifdef __APPLE__
#include <os/log.h>
#endif

ConsoleListener::ConsoleListener() : bUseColor(false) {}
ConsoleListener::~ConsoleListener() = default;

void ConsoleListener::Open(bool, int, int, const char*) {}
void ConsoleListener::UpdateHandle() {}
void ConsoleListener::Close() {}
bool ConsoleListener::IsOpen() { return true; }
void ConsoleListener::LetterSpace(int, int) {}
void ConsoleListener::BufferWidthHeight(int, int, int, int, bool) {}
void ConsoleListener::PixelSpace(int, int, int, int, bool) {}
void ConsoleListener::ClearScreen(bool) {}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text) {
    if (!text || !*text) {
        return;
    }
#ifdef __APPLE__
    os_log_type_t log_type = OS_LOG_TYPE_DEFAULT;
    switch (level) {
    case LogTypes::LERROR:
        log_type = OS_LOG_TYPE_ERROR;
        break;
    case LogTypes::LWARNING:
        log_type = OS_LOG_TYPE_DEFAULT;
        break;
    case LogTypes::LINFO:
    case LogTypes::LNOTICE:
        log_type = OS_LOG_TYPE_INFO;
        break;
    case LogTypes::LDEBUG:
    default:
        log_type = OS_LOG_TYPE_DEBUG;
        break;
    }
    os_log_with_type(OS_LOG_DEFAULT, log_type, "%{public}s", text);
#else
    std::fputs(text, stdout);
    std::fflush(stdout);
#endif
}
