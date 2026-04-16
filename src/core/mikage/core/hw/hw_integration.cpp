// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "hw_integration.h"
#include "../../common/log.h"

namespace HW {

// ============================================================================
// Hardware Integration Implementation
// ============================================================================

HardwareIntegration* g_hardware_integration = nullptr;

HardwareIntegration::HardwareIntegration() 
    : initialized(false), total_cpu_cycles(0) {
}

HardwareIntegration::~HardwareIntegration() {
    if (initialized) {
        Shutdown();
    }
}

void HardwareIntegration::Initialize(u64 cpu_clock_rate) {
    NOTICE_LOG(HW, "Initializing Hardware Layer");
    
    // Initialize all hardware components
    Timer::InitTimerManager(cpu_clock_rate);
    InterruptController::InitInterruptManager();
    LCD::InitLCDController();
    DMA::InitDMAController();
    
    initialized = true;
    NOTICE_LOG(HW, "Hardware Layer initialized successfully");
}

void HardwareIntegration::Shutdown() {
    if (!initialized) return;
    
    NOTICE_LOG(HW, "Shutting down Hardware Layer");
    
    // Shutdown all hardware components
    DMA::ShutdownDMAController();
    LCD::ShutdownLCDController();
    InterruptController::ShutdownInterruptManager();
    Timer::ShutdownTimerManager();
    
    initialized = false;
    NOTICE_LOG(HW, "Hardware Layer shutdown complete");
}

void HardwareIntegration::Update(u64 cpu_cycles) {
    if (!initialized) return;
    
    total_cpu_cycles += cpu_cycles;
    
    // Update all hardware components
    if (Timer::g_timer_manager) {
        Timer::g_timer_manager->Advance(cpu_cycles);
    }
    
    if (LCD::g_lcd_controller) {
        LCD::g_lcd_controller->Update();
    }
    
    if (DMA::g_dma_controller) {
        DMA::g_dma_controller->Update();
    }
    
    // Check for interrupts
    if (InterruptController::g_interrupt_manager) {
        if (InterruptController::g_interrupt_manager->ShouldProcessInterrupt()) {
            // Interrupt pending - CPU will handle in next cycle
        }
    }
}

void HardwareIntegration::Reset() {
    NOTICE_LOG(HW, "Resetting Hardware Layer");
    
    if (Timer::g_timer_manager) {
        Timer::g_timer_manager->Reset();
    }
    
    if (LCD::g_lcd_controller) {
        // Reset LCD
    }
    
    if (DMA::g_dma_controller) {
        DMA::g_dma_controller->Reset();
    }
    
    if (InterruptController::g_interrupt_manager) {
        InterruptController::g_interrupt_manager->Reset();
    }
    
    total_cpu_cycles = 0;
}

void InitializeHardwareLayer(u64 cpu_clock_rate) {
    if (!g_hardware_integration) {
        g_hardware_integration = new HardwareIntegration();
    }
    g_hardware_integration->Initialize(cpu_clock_rate);
}

void ShutdownHardwareLayer() {
    if (g_hardware_integration) {
        g_hardware_integration->Shutdown();
        delete g_hardware_integration;
        g_hardware_integration = nullptr;
    }
}

} // namespace HW
