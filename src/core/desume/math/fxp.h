#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t fx32_mul(int32_t a, int32_t b) {
    return (int64_t)a * (int64_t)b;
}

static inline int64_t fx32_shiftup(int32_t a) {
    return ((int64_t)a) << 12;
}

static inline int32_t fx32_shiftdown(int64_t a) {
    return (int32_t)(a >> 12);
}

#ifdef __cplusplus
}
#endif
