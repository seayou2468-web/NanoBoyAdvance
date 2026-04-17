#pragma once
#include <cassert>
#include "log.h"
#define ASSERT(x) assert(x)
#define DEBUG_ASSERT(x) assert(x)
#define UNREACHABLE() assert(false)
