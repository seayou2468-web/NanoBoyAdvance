// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "../common/common_types.h"
#include "../common/log.h"
#include "../common/symbols.h"

#include "core.h"
#include "mem_map.h"
#include "system.h"
#include "./hw/hw.h"
#include "./arm/disassembler/arm_disasm.h"
#include "./arm/interpreter/arm_interpreter.h"

#include "./hle/kernel/thread.h"

namespace Core {

ARM_Disasm*     g_disasm    = NULL; ///< ARM disassembler
ARM_Interface*  g_app_core  = NULL; ///< ARM11 application core
ARM_Interface*  g_sys_core  = NULL; ///< ARM11 system (OS) core
static const char* g_halt_reason = nullptr;

// Forward reference to Memory system
class System;

void Start() {
    System::UpdateState(System::STATE_RUNNING);
    RunLoop();
}

/// Run the core CPU loop
void RunLoop() {
    while (System::g_state == System::STATE_RUNNING){
        if (!g_app_core) {
            Halt("application core is not initialized");
            break;
        }
        g_app_core->Run(100);
        HW::Update();
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    if (!g_app_core) {
        Halt("application core is not initialized");
        return;
    }
    g_app_core->Step();
    HW::Update();
    Kernel::Reschedule();
}

/// Halt the core
void Halt(const char *msg) {
    g_halt_reason = msg;
    ERROR_LOG(MASTER_LOG, "Core halted: %s", msg ? msg : "(no reason)");
    System::UpdateState(System::STATE_HALTED);
}

/// Kill the core
void Stop() {
    NOTICE_LOG(MASTER_LOG, "Core stop requested");
    System::UpdateState(System::STATE_DIE);
}

/**
 * Initialize the core with legacy mode (backwards compatibility)
 * Uses old ARMul_State initialization
 */
int Init() {
    NOTICE_LOG(MASTER_LOG, "Initializing core (legacy compatibility mode)");
    g_halt_reason = nullptr;

    g_disasm = new ARM_Disasm();
    g_app_core = new ARM_Interpreter();
    g_sys_core = new ARM_Interpreter();

    NOTICE_LOG(MASTER_LOG, "Core initialized OK (legacy mode)");
    System::UpdateState(System::STATE_IDLE);
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
    g_halt_reason = nullptr;

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
    
    constexpr u32 kUser32Mode = 16; // USER32MODE
    constexpr u32 kSvc32Mode = 19;  // SVC32MODE
    app_core->InitializeWithSystem(system, memory_system, kUser32Mode);
    sys_core->InitializeWithSystem(system, memory_system, kSvc32Mode);
    
    g_app_core = app_core;
    g_sys_core = sys_core;

    NOTICE_LOG(MASTER_LOG, "Core initialized OK (Cytrus DynCom mode with full CPU support)");
    System::UpdateState(System::STATE_IDLE);
    return 0;
}

void Shutdown() {
    delete g_disasm;
    if (g_app_core) delete g_app_core;
    if (g_sys_core) delete g_sys_core;
    g_app_core = nullptr;
    g_sys_core = nullptr;
    g_disasm = nullptr;

    NOTICE_LOG(MASTER_LOG, "Core shutdown OK");
    System::UpdateState(System::STATE_NULL);
}

} // namespace
