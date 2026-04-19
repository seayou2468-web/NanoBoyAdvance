#pragma once

#include <cstdio>
#include <cstdlib>

#ifndef ASSERT
#define ASSERT(cond)                                                                               \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "ASSERT failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (0)
#endif

#ifndef ASSERT_MSG
#define ASSERT_MSG(cond, fmt, ...)                                                                 \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "ASSERT failed: %s (%s:%d): " fmt "\n", #cond, __FILE__, __LINE__, ##__VA_ARGS__); \
            std::abort();                                                                          \
        }                                                                                          \
    } while (0)
#endif

#define DEBUG_ASSERT(cond) ASSERT(cond)
#define DEBUG_ASSERT_MSG(cond, fmt, ...) ASSERT_MSG(cond, fmt, ##__VA_ARGS__)
#define UNREACHABLE() ASSERT_MSG(false, "Unreachable code")
#define UNREACHABLE_MSG(...) ASSERT_MSG(false, __VA_ARGS__)
#define UNIMPLEMENTED() ASSERT_MSG(false, "Unimplemented code")
#define UNIMPLEMENTED_MSG(...) ASSERT_MSG(false, __VA_ARGS__)
