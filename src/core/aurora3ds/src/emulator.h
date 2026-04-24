#ifndef EMULATOR_H
#define EMULATOR_H

#include <setjmp.h>

#include "3ds.h"
#include "common.h"

enum {
    EXC_OK,
    EXC_MEM,
    EXC_EXIT,
    EXC_ERRF,
    EXC_BREAK,
};

typedef struct {
    bool ignore_null;
    bool detectRegion;
    bool needs_swkbd;

    int audiomode;
    int language;
    int region;
    char username[0x1c];
    char* romfilenoext;

    jmp_buf exceptionJmp;

    E3DS system;
} EmulatorState;

extern EmulatorState ctremu;

#endif
