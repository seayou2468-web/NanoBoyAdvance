// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../common/compat/common/common_types.h"
#include "arm/arm_interface.h"
#include "arm/arm_dyncom.h"
#include "loader/rom_manager.h"
#include "hw/hw.h"
#include "hle/kernel/kernel.h"
#include "gpu/pica200.h"
#include "../common/compat/core/memory.h"
#include "../common/compat/core/core_timing.h"
#include <memory>

namespace Core {

// ============================================================================
// Emulation Core / System
// ============================================================================

class Emulator {
public:
    Emulator();
    ~Emulator();
    
    // Initialize emulator
    bool Initialize();
    
    // Load ROM and prepare for execution
    bool LoadROM(const std::string& filename);
    
    // Start emulation
    void Run();
    
    // Pause emulation
    void Pause();
    
    // Stop emulation
    void Stop();
    
    // Step one instruction
    void Step();
    
    // Check if running
    bool IsRunning() const { return is_running; }
    
    // Get CPU core
    ARM_Interface* GetCPUCore() { return cpu_core.get(); }
    
    // Get memory pointer
    u8* GetMemory() { return memory.data(); }
    
    // Get memory size
    u32 GetMemorySize() const { return memory.size(); }

private:
    // Core components
    std::unique_ptr<ARM_Interface> cpu_core;
    std::unique_ptr<Memory::MemorySystem> memory_system;
    std::shared_ptr<Core::Timing::Timer> timer;
    std::unique_ptr<Loader::ROMManager> rom_manager;
    std::unique_ptr<HW::Hardware> hardware;
    std::unique_ptr<HLE::Kernel::Kernel> kernel;
    std::unique_ptr<GPU::PICA200::PICA200> gpu;
    
    // Memory (256MB for 3DS)
    std::vector<u8> memory;
    
    // Status
    bool is_running;
    bool is_initialized;
    
    // Initialization helpers
    bool InitializeCPU();
    bool InitializeMemory();
    bool InitializeHW();
    bool InitializeKernel();
    bool InitializeGPU();
};

} // namespace Core
