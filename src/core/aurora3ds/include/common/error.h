#pragma once

#include "common/assert.h"

#ifndef ASSERT_MSG
#define ASSERT_MSG(cond, ...) ASSERT(cond)
#endif

#ifndef UNREACHABLE
#define UNREACHABLE() ASSERT(false)
#endif
