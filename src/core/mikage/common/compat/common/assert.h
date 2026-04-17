// iOS Compatibility Layer for Common Assertions
// Simple assert implementation
#pragma once

#include <cassert>
#include <cstdlib>

#ifdef NDEBUG
#define ASSERT(condition) ((void)0)
#define ASSERT_MSG(cond, ...) ((void)0)
#else
#define ASSERT(condition) assert(condition)
#define ASSERT_MSG(cond, msg) assert(cond)
#endif

// Unimplemented assertion
#define ASSERT_NOT_IMPLEMENTED() assert(!"Not implemented")

// Unreachable code assertion  
#define UNREACHABLE() std::abort()
