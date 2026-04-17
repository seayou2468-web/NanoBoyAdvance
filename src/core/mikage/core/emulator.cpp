// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "emulator.h"
#include "../common/compat/common/logging/log.h"
#include "../common/compat/core/core.h"
#include <cstring>

namespace Core {

// ============================================================================
// Emulator Implementation
// ============================================================================

Emulator::Emulator()
    : is_running(false), is_initialized(false) {
    
    // Allocate 256MB memory for 3DS
    memory.resize(256 * 1024 * 1024);
    std::memset(memory.data(), 0, memory.size());
    
    NOTICE_LOG(CORE, "Emulator created (256MB memory)");
}

Emulator::~Emulator() {
    if (is_initialized) {
        Stop();
    }
}

bool Emulator::Initialize() {
    NOTICE_LOG(CORE, "Initializing emulator...");
    
    if (!InitializeMemory()) {
        ERROR_LOG(CORE, "Failed to initialize memory");
        return false;
    }
    
    if (!InitializeCPU()) {
        ERROR_LOG(CORE, "Failed to initialize CPU");
        return false;
    }
    
    if (!InitializeHW()) {
        ERROR_LOG(CORE, "Failed to initialize hardware");
        return false;
    }
    
    if (!InitializeKernel()) {
        ERROR_LOG(CORE, "Failed to initialize kernel");
        return false;
    }
    
    if (!InitializeGPU()) {
        ERROR_LOG(CORE, "Failed to initialize GPU");
        return false;
    }
    
    // Initialize ROM manager
    rom_manager = std::make_unique<Loader::ROMManager>();
    
    is_initialized = true;
    NOTICE_LOG(CORE, "Emulator initialized successfully");
    return true;
}

bool Emulator::InitializeMemory() {
    NOTICE_LOG(CORE, "Initializing memory system (256MB)");
    // Memory already allocated in constructor
    return true;
}

bool Emulator::InitializeCPU() {
    LOG_DEBUG(Core_ARM11, "Initializing ARM11 DynCom CPU core (JIT-free)");
    
    // Create memory system for CPU
    memory_system = std::make_unique<Memory::MemorySystem>();
    if (!memory_system) {
        LOG_ERROR(Core_ARM11, "Failed to create memory system");
        return false;
    }
    
    // Create timing system for CPU
    timer = std::make_shared<Core::Timing::Timer>();
    if (!timer) {
        LOG_ERROR(Core_ARM11, "Failed to create timer");
        return false;
    }
    
    // Create ARM DynCom core (JIT-free interpreter)
    try {
        Core::System sys;  // Pass minimal system reference
        cpu_core = std::make_unique<Core::ARM_DynCom>(
            sys,                              // System reference
            *memory_system,                   // Memory system
            PrivilegeMode::USER32MODE,        // Initial privilege
            0,                                // CPU ID
            timer                             // Timing reference
        );
    } catch (const std::exception& e) {
        LOG_ERROR(Core_ARM11, "Failed to create ARM DynCom: %s", e.what());
        return false;
    }
    
    if (!cpu_core) {
        LOG_ERROR(Core_ARM11, "Failed to create ARM DynCom core");
        return false;
    }
    
    LOG_DEBUG(Core_ARM11, "ARM11 DynCom CPU core initialized (no JIT)");
    return true;
}

bool Emulator::InitializeHW() {
    NOTICE_LOG(CORE, "Initializing hardware controllers");
    
    hardware = std::make_unique<HW::Hardware>();
    if (!hardware) {
        ERROR_LOG(CORE, "Failed to create hardware");
        return false;
    }
    
    NOTICE_LOG(CORE, "Hardware initialized");
    return true;
}

bool Emulator::InitializeKernel() {
    NOTICE_LOG(CORE, "Initializing kernel");
    
    kernel = std::make_unique<HLE::Kernel::Kernel>();
    if (!kernel) {
        ERROR_LOG(CORE, "Failed to create kernel");
        return false;
    }
    
    NOTICE_LOG(CORE, "Kernel initialized");
    return true;
}

bool Emulator::InitializeGPU() {
    NOTICE_LOG(CORE, "Initializing PICA200 GPU");
    
    gpu = std::make_unique<GPU::PICA200::PICA200>();
    if (!gpu) {
        ERROR_LOG(CORE, "Failed to create GPU");
        return false;
    }
    
    NOTICE_LOG(CORE, "GPU initialized");
    return true;
}

bool Emulator::LoadROM(const std::string& filename) {
    if (!is_initialized) {
        ERROR_LOG(CORE, "Emulator not initialized");
        return false;
    }
    
    NOTICE_LOG(CORE, "Loading ROM: %s", filename.c_str());
    
    if (!rom_manager->LoadROM(filename)) {
        ERROR_LOG(CORE, "Failed to load ROM");
        return false;
    }
    
    // Load program into memory
    if (!rom_manager->LoadIntoMemory(memory.data(), memory.size())) {
        ERROR_LOG(CORE, "Failed to load program into memory");
        return false;
    }
    
    u32 entry_point = rom_manager->GetEntryPoint();
    NOTICE_LOG(CORE, "ROM loaded successfully");
    NOTICE_LOG(CORE, "Entry point: 0x%x", entry_point);
    NOTICE_LOG(CORE, "Program: %s", rom_manager->GetProgramName().c_str());
    
    // Set CPU entry point
    if (cpu_core) {
        cpu_core->SetPC(entry_point);
        NOTICE_LOG(CORE, "CPU entry point set to 0x%x", entry_point);
    }
    
    return true;
}

void Emulator::Run() {
    if (!is_initialized) {
        ERROR_LOG(CORE, "Emulator not initialized");
        return;
    }
    
    is_running = true;
    NOTICE_LOG(CORE, "Emulator running...");
    
    // Main execution loop
    while (is_running) {
        // Execute one CPU cycle
        if (cpu_core) {
            cpu_core->ExecuteInstructions(1);
        }
        
        // Update hardware (timers, interrupts, etc.)
        if (hardware) {
            hardware->Update();
        }
        
        // Update GPU
        if (gpu) {
            gpu->RenderFrame();
        }
    }
    
    NOTICE_LOG(CORE, "Emulator stopped");
}

void Emulator::Pause() {
    is_running = false;
    NOTICE_LOG(CORE, "Emulator paused");
}

void Emulator::Stop() {
    is_running = false;
    is_initialized = false;
    
    // Shutdown components
    gpu = nullptr;
    kernel = nullptr;
    hardware = nullptr;
    cpu_core = nullptr;
    rom_manager = nullptr;
    
    NOTICE_LOG(CORE, "Emulator stopped");
}

void Emulator::Step() {
    if (!is_initialized) {
        ERROR_LOG(CORE, "Emulator not initialized");
        return;
    }
    
    if (cpu_core) {
        cpu_core->ExecuteInstructions(1);
        NOTICE_LOG(CORE, "CPU step executed");
    }
}

} // namespace Core
