#ifndef ARM_CORE_H
#define ARM_CORE_H

#include "common.h"

typedef struct {
    struct {
        u32 r[16];
        u32 cpsr;
    } ctx;
} ArmCore;

static inline void cpu_print_state(ArmCore* cpu) {
    (void)cpu;
}

#endif
