#include "core/arm/interpreter/arm_interpreter.h"
#include "core/memory.h"
#include "core/core.h"
#include "core/system.h"
#include "common/log.h"

namespace Core {
ARM_Interface* g_app_core = nullptr;
ARM_Interface* g_sys_core = nullptr;

int Init() {
    g_app_core = new ARM_Interpreter();
    g_sys_core = new ARM_Interpreter();
    return 0;
}

int InitWithSystem(System& system) {
    Memory::MemorySystem& memory_system = Memory::GetMemorySystem();
    auto* app_core = new ARM_Interpreter();
    auto* sys_core = new ARM_Interpreter();
    app_core->InitializeWithSystem(system, memory_system);
    sys_core->InitializeWithSystem(system, memory_system);
    g_app_core = app_core;
    g_sys_core = sys_core;
    return 0;
}

void Start() {}
void RunLoop() {}
void SingleStep() {}
void Halt(const char* msg) {}
void Stop() {}
void Shutdown() {}
}
