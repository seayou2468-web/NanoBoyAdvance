// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#if __APPLE__
#import <TargetConditionals.h>
#endif

#include <algorithm>
#include <array>
#include <string_view>

#include "common/logging/formatter.h"
#include "common/logging/types.h"

namespace Common::Log {

// trims up to and including the last of ../, ..\, src/, src\ in a string
constexpr const char* TrimSourcePath(std::string_view source) {
    const auto rfind = [source](const std::string_view match) {
        return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
    };
#if TARGET_OS_IOS
    auto idx = std::max({rfind("Core/"), rfind("Cytrus/"), rfind("../"), rfind("..\\")});
#else
    auto idx = std::max({rfind("src/"), rfind("src\\"), rfind("../"), rfind("..\\")});
#endif
    return source.data() + idx;
}

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, fmt::string_view format,
                       const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, fmt::format_string<Args...> format, const Args&... args) {
    FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format,
                      fmt::make_format_args(args...));
}

inline void Stop() {}

} // namespace Common::Log

// Logging is disabled for this iOS-focused build.
#define LOG_GENERIC(log_class, log_level, ...) (void(0))
#define LOG_TRACE(log_class, ...) (void(0))
#define LOG_DEBUG(log_class, ...) (void(0))
#define LOG_INFO(log_class, ...) (void(0))
#define LOG_WARNING(log_class, ...) (void(0))
#define LOG_ERROR(log_class, ...) (void(0))
#define LOG_CRITICAL(log_class, ...) (void(0))
