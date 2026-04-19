// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "timer.h"
#include "interrupt.h"
#include "lcd.h"
#include "dma.h"
#include "../../common/common_types.h"

namespace HW {

// ============================================================================
// Hardware Manager (Master)
// ============================================================================

class HardwareIntegration {
public:
    HardwareIntegration();
    ~HardwareIntegration();
    
    // Initialize all hardware
    void Initialize(u64 cpu_clock_rate);
    
    // Shutdown all hardware
    void Shutdown();
    
    // Get component access
    Timer::TimerManager* GetTimerManager() { return Timer::g_timer_manager; }
    InterruptController::InterruptManager* GetInterruptManager() {
        return InterruptController::g_interrupt_manager;
    }
    LCD::LCDController* GetLCDController() { return LCD::g_lcd_controller; }
    DMA::DMAController* GetDMAController() { return DMA::g_dma_controller; }
    
    // Global update (called each cycle)
    void Update(u64 cpu_cycles);
    
    // Reset all hardware
    void Reset();

private:
    bool initialized;
    u64 total_cpu_cycles;
};

// ============================================================================
// Global Hardware Manager
// ============================================================================

extern HardwareIntegration* g_hardware_integration;

// Initialization functions
void InitializeHardwareLayer(u64 cpu_clock_rate = 268111200);  // 3DS CPU clock
void ShutdownHardwareLayer();

} // namespace HW
