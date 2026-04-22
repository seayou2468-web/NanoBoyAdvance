#pragma once
#include "../helpers.hpp"

[[noreturn]] inline void Crash() {
    Helpers::panic("Crash() called");
}
