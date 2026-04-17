// iOS Compatibility Layer for Logging
// Replaces Citra's complex logging system with standard C++ output
#pragma once

#include <iostream>
#include <cstdio>

#ifdef __APPLE__
#include <os/log.h>
#define USE_OS_LOG 1
#else
#define USE_OS_LOG 0
#endif

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

enum class LogCategory {
    Core_ARM11 = 0,
    Core,
    HLE,
    Loader,
    GPU,
    Memory,
    Unknown
};

// Simplified logging that writes to stderr or NSLog on iOS
inline void LogMessage(LogLevel level, LogCategory category, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
#if USE_OS_LOG
    // iOS NSLog-style output
    os_log_type_t os_level = OS_LOG_TYPE_DEFAULT;
    switch (level) {
        case LogLevel::Trace:
        case LogLevel::Debug:
            os_level = OS_LOG_TYPE_DEBUG;
            break;
        case LogLevel::Info:
            os_level = OS_LOG_TYPE_INFO;
            break;
        case LogLevel::Warning:
            os_level = OS_LOG_TYPE_WARNING;
            break;
        case LogLevel::Error:
        case LogLevel::Critical:
            os_level = OS_LOG_TYPE_ERROR;
            break;
    }
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    os_log_with_type(OS_LOG_DEFAULT, os_level, "%{public}s", buffer);
#else
    // Standard stderr output
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
#endif
    
    va_end(args);
}

// Macros to match Citra's logging interface
#define LOG_TRACE(category, ...) \
    LogMessage(LogLevel::Trace, LogCategory::category, __VA_ARGS__)
#define LOG_DEBUG(category, ...) \
    LogMessage(LogLevel::Debug, LogCategory::category, __VA_ARGS__)
#define LOG_INFO(category, ...) \
    LogMessage(LogLevel::Info, LogCategory::category, __VA_ARGS__)
#define LOG_WARNING(category, ...) \
    LogMessage(LogLevel::Warning, LogCategory::category, __VA_ARGS__)
#define LOG_ERROR(category, ...) \
    LogMessage(LogLevel::Error, LogCategory::category, __VA_ARGS__)
#define LOG_CRITICAL(category, ...) \
    LogMessage(LogLevel::Critical, LogCategory::category, __VA_ARGS__)
