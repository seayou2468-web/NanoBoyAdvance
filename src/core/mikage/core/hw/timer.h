// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <functional>
#include <vector>
#include <queue>

namespace HW {
namespace Timer {

// ============================================================================
// Timer Registers (3DS ARM11)
// ============================================================================
//
// 3DS has 4 independent 16-bit timers (Timer 0, 1, 2, 3)
// Each can operate in different modes:
//   - OneShot: Timer counts down and stops
//   - Free-run: Timer counts down and reloads
//   - Prescaler: Divide CPU clock by 1, 64, 256, or 1024
//

enum class TimerPrescaler {
    Divide1 = 0,    // CPU clock / 1 (268.111 MHz on 3DS)
    Divide64 = 1,   // CPU clock / 64
    Divide256 = 2,  // CPU clock / 256
    Divide1024 = 3, // CPU clock / 1024
};

enum class TimerMode {
    OneShot = 0,  // Count down once
    FreeRun = 1,  // Count down and reload
};

struct TimerControl {
    u32 raw;
    
    TimerPrescaler GetPrescaler() const {
        return static_cast<TimerPrescaler>((raw >> 0) & 0x3);
    }
    
    TimerMode GetMode() const {
        return static_cast<TimerMode>((raw >> 2) & 0x1);
    }
    
    bool IsEnabled() const {
        return (raw >> 7) & 0x1;
    }
    
    void SetPrescaler(TimerPrescaler p) {
        raw = (raw & ~0x3) | (static_cast<u32>(p) & 0x3);
    }
    
    void SetMode(TimerMode m) {
        raw = (raw & ~0x4) | ((static_cast<u32>(m) & 0x1) << 2);
    }
    
    void SetEnabled(bool enabled) {
        raw = (raw & ~0x80) | ((enabled ? 1 : 0) << 7);
    }
};

// ============================================================================
// Individual Timer
// ============================================================================

class Timer {
public:
    Timer(u32 id, u64 parent_clock_rate);
    ~Timer() = default;
    
    // Register access
    u16 GetCounter() const { return counter; }
    void SetCounter(u16 value);
    
    u16 GetReload() const { return reload_value; }
    void SetReload(u16 value);
    
    u32 GetControl() const { return control.raw; }
    void SetControl(u32 value);
    
    u32 GetInterruptControl() const { return interrupt_enable; }
    void SetInterruptControl(u32 value) { interrupt_enable = value; }
    
    // Timer execution
    void UpdateCycles(u64 cpu_cycles);
    bool HasInterrupt() const { return interrupt_pending; }
    void ClearInterrupt() { interrupt_pending = false; }
    
    // Callback
    typedef std::function<void(u32)> InterruptCallback;
    void SetInterruptCallback(InterruptCallback callback);

private:
    u32 id;
    u64 parent_clock_rate;
    
    u16 counter;
    u16 reload_value;
    TimerControl control;
    
    u64 cycles_elapsed;
    u32 interrupt_enable;
    bool interrupt_pending;
    
    InterruptCallback on_interrupt;
    
    u32 GetPrescaleDivisor() const;
};

// ============================================================================
// Timer Manager (4 timers)
// ============================================================================

class TimerManager {
public:
    TimerManager(u64 cpu_clock_rate);
    ~TimerManager() = default;
    
    // Access individual timers
    Timer& GetTimer(u32 id);
    
    // Global time update
    void Advance(u64 cpu_cycles);
    
    // Check for pending interrupts
    std::vector<u32> GetPendingInterrupts();
    
    // Reset all timers
    void Reset();

private:
    std::vector<Timer> timers;
    u64 total_cpu_cycles;
};

// Global timer manager
extern TimerManager* g_timer_manager;

void InitTimerManager(u64 cpu_clock_rate = 268111200);  // 3DS CPU clock
void ShutdownTimerManager();

} // namespace Timer
} // namespace HW
