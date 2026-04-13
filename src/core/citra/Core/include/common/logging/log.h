#pragma once

#include <cstdio>
#include <utility>
#include "../../../../includes/libfmt.xcframework/fmt/format.h"

namespace Common::Log {

template <typename... Args>
inline void Write(const char* level, const char* channel, fmt::format_string<Args...> fmt_str,
                  Args&&... args) {
    fmt::print(stderr, "[{}][{}] {}\n", level, channel,
               fmt::format(fmt_str, std::forward<Args>(args)...));
}

inline void WriteRaw(const char* level, const char* channel, const char* message) {
    fmt::print(stderr, "[{}][{}] {}\n", level, channel, message);
}

} // namespace Common::Log

#define LOG_TRACE(channel, ...) Common::Log::Write("TRACE", #channel, __VA_ARGS__)
#define LOG_DEBUG(channel, ...) Common::Log::Write("DEBUG", #channel, __VA_ARGS__)
#define LOG_INFO(channel, ...) Common::Log::Write("INFO", #channel, __VA_ARGS__)
#define LOG_WARNING(channel, ...) Common::Log::Write("WARN", #channel, __VA_ARGS__)
#define LOG_ERROR(channel, ...) Common::Log::Write("ERROR", #channel, __VA_ARGS__)
#define LOG_CRITICAL(channel, ...) Common::Log::Write("CRITICAL", #channel, __VA_ARGS__)
