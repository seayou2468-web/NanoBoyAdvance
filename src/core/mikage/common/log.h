#pragma once
#include <cstdio>
#include <assert.h>

#ifndef assert
#include <assert.h>
#endif

namespace Log {
template<typename... Args>
inline void stub(Args... args) { }
}

#define LOG_TRACE(...) ::Log::stub(__VA_ARGS__)
#define LOG_DEBUG(...) ::Log::stub(__VA_ARGS__)
#define LOG_INFO(...) ::Log::stub(__VA_ARGS__)
#define LOG_WARNING(...) ::Log::stub(__VA_ARGS__)
#define LOG_ERROR(...) ::Log::stub(__VA_ARGS__)
#define NOTICE_LOG(...) ::Log::stub(__VA_ARGS__)
#define WARN_LOG(...) ::Log::stub(__VA_ARGS__)
#define ERROR_LOG(...) ::Log::stub(__VA_ARGS__)
#define ASSERT_MSG(cond, ...) assert(cond)
#define _dbg_assert_msg_(cat, cond, ...) assert(cond)

enum LogCategory {
    Core_ARM11_Cat = 0,
    VIDEO_Cat = 1,
    COMMON_Cat = 2,
    HLENamespace_Cat = 3,
};
#define Core_ARM11 ::Core_ARM11_Cat
#define VIDEO ::VIDEO_Cat
#define COMMON ::COMMON_Cat
#define HLE ::HLENamespace_Cat
