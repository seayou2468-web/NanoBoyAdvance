#include "emulator.h"

EmulatorState ctremu = {
    .ignore_null = false,
    .detectRegion = false,
    .needs_swkbd = false,
    .audiomode = 0,
    .language = 1,
    .region = 1,
    .username = "Aurora",
    .romfilenoext = (char*)"",
};
