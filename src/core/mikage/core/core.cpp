// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "../common/common_types.h"
#include "../common/log.h"
#include "../common/symbols.h"
#include "../common/compat/core/core.h"

#include "core.h"
#include "memory.h"
#include "mem_map.h"
#include "./hw/hw.h"
#include "./arm/disassembler/arm_disasm.h"
#include "./arm/interpreter/arm_interpreter.h"

#include "./hle/kernel/thread.h"

namespace Core {

ARM_Disasm*     g_disasm    = NULL; ///< ARM disassembler
ARM_Interface*  g_app_core  = NULL; ///< ARM11 application core
ARM_Interface*  g_sys_core  = NULL; ///< ARM11 system (OS) core
std::unique_ptr<Core::System> g_legacy_system; ///< Owned System for legacy Init()

// Forward reference to Memory system
class System;

/// Run the core CPU loop
void RunLoop() {
    for (;;){
        g_app_core->Run(100);
        HW::Update();
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    g_app_core->Step();
    HW::Update();
    Kernel::Reschedule();
}

/// Halt the core
void Halt(const char *msg) {
    // TODO(ShizZy): ImplementMe
}

/// Kill the core
void Stop() {
    // TODO(ShizZy): ImplementMe
}

/**
 * Initialize the core with legacy mode (backwards compatibility)
 * Uses old ARMul_State initialization
 */
int Init() {
    NOTICE_LOG(MASTER_LOG, "Initializing core (legacy compatibility mode)");

    g_disasm = new ARM_Disasm();
    g_legacy_system = std::make_unique<Core::System>();

    auto* app_core = new ARM_Interpreter();
    auto* sys_core = new ARM_Interpreter();

    Memory::MemorySystem& memory_system = Memory::GetMemorySystem();
    app_core->InitializeWithSystem(*g_legacy_system, memory_system);
    sys_core->InitializeWithSystem(*g_legacy_system, memory_system);

    g_app_core = app_core;
    g_sys_core = sys_core;

    NOTICE_LOG(MASTER_LOG, "Core initialized OK (legacy mode)");
    return 0;
}

/**
 * Initialize the core with System reference
 * This enables full Cytrus DynCom CPU support with proper memory integration
 * 
 * @param system Reference to the System instance containing Memory and Timer
 */
int InitWithSystem(class System& system) {
    NOTICE_LOG(MASTER_LOG, "Initializing core with Cytrus DynCom CPU (System-aware mode)");

    if (g_app_core || g_sys_core) {
        NOTICE_LOG(MASTER_LOG, "Warning: Core already initialized, cleaning up");
        if (g_disasm) delete g_disasm;
        if (g_app_core) delete g_app_core;
        if (g_sys_core) delete g_sys_core;
    }

    g_disasm = new ARM_Disasm();
    
    // Create new ARM_Interpreter instances
    ARM_Interpreter* app_core = new ARM_Interpreter();
    ARM_Interpreter* sys_core = new ARM_Interpreter();
    
    // Initialize with System and Memory for full DynCom CPU support
    // Get Memory system reference from system
    Memory::MemorySystem& memory_system = Memory::GetMemorySystem();
    
    app_core->InitializeWithSystem(system, memory_system);
    sys_core->InitializeWithSystem(system, memory_system);
    
    g_app_core = app_core;
    g_sys_core = sys_core;

    NOTICE_LOG(MASTER_LOG, "Core initialized OK (Cytrus DynCom mode with full CPU support)");
    return 0;
}

void Shutdown() {
    delete g_disasm;
    if (g_app_core) delete g_app_core;
    if (g_sys_core) delete g_sys_core;
    g_legacy_system.reset();

    NOTICE_LOG(MASTER_LOG, "Core shutdown OK");
}

} // namespace
