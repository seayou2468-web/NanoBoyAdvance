#pragma once
#include "../arm_defs.hpp"
#include "../common/common_types.h"
namespace Core::Timing { class Timer { public: s64 GetDowncount() const { return 0; } void AddTicks(u64) {} }; }
