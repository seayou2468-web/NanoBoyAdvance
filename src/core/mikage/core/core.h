// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "./arm/arm_interface.h"
#include "./arm/interpreter/armdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Kernel {
class KernelSystem;
}

namespace Core {

class Timing;
class Movie;
class PerfStats;

class System { // Cytrus CPU migration shim declaration.
public:
    static System& GetInstance();

    Timing& CoreTiming();
    Movie& Movie();
    ::Kernel::KernelSystem& Kernel();

    PerfStats* perf_stats{};
};
class ARM_Interpreter; // Forward declare for System integration

////////////////////////////////////////////////////////////////////////////////////////////////////

extern ARM_Interface*   g_app_core;     ///< ARM11 application core
extern ARM_Interface*   g_sys_core;     ///< ARM11 system (OS) core

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Start the core
void Start();

/// Run the core CPU loop
void RunLoop();

/// Step the CPU one instruction
void SingleStep();

/// Halt the core
void Halt(const char *msg);

/// Kill the core
void Stop();

/// Initialize the core with System reference (required for Cytrus CPU integration)
int Init();

/// Initialize the core with System instance for full DynCom support
int InitWithSystem(class System& system);

/// Shutdown the core
void Shutdown();

} // namespace
