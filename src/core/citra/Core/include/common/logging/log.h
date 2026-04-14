#pragma once

namespace Common::Log {

enum class Class {
    Kernel,
};

enum class Level {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

} // namespace Common::Log

#define LOG_TRACE(channel, ...) ((void)0)
#define LOG_DEBUG(channel, ...) ((void)0)
#define LOG_INFO(channel, ...) ((void)0)
#define LOG_WARNING(channel, ...) ((void)0)
#define LOG_ERROR(channel, ...) ((void)0)
#define LOG_CRITICAL(channel, ...) ((void)0)
#define LOG_GENERIC(channel, level, ...) ((void)0)
