#pragma once
#include "../arm_defs.hpp"
namespace Core::Timing { class Timer { public: s64 GetDowncount() const { return 0; } void AddTicks(u64) {} }; }
