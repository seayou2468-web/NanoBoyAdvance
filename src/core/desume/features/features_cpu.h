#pragma once

#include <thread>

static inline int cpu_features_get_core_amount(void) {
    unsigned int cores = std::thread::hardware_concurrency();
    return cores > 0 ? (int)cores : 1;
}
