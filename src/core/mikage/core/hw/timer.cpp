// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "timer.h"
#include "../../common/log.h"
#include <algorithm>

namespace HW {
namespace Timer {

// ============================================================================
// Timer Implementation
// ============================================================================

Timer::Timer(u32 timer_id, u64 parent_clock_rate)
    : id(timer_id), parent_clock_rate(parent_clock_rate),
      counter(0), reload_value(0), cycles_elapsed(0),
      interrupt_enable(0), interrupt_pending(false) {
    control.raw = 0;
}

void Timer::SetCounter(u16 value) {
    counter = value;
    cycles_elapsed = 0;
    interrupt_pending = false;
    NOTICE_LOG(HW, "Timer %d: Counter = 0x%04x", id, value);
}

void Timer::SetReload(u16 value) {
    reload_value = value;
    NOTICE_LOG(HW, "Timer %d: Reload = 0x%04x", id, value);
}

void Timer::SetControl(u32 value) {
    control.raw = value;
    NOTICE_LOG(HW, "Timer %d: Control = 0x%02x (prescaler=%d, mode=%d, enabled=%d)",
               id, value, static_cast<u32>(control.GetPrescaler()),
               static_cast<u32>(control.GetMode()), control.IsEnabled());
}

u32 Timer::GetPrescaleDivisor() const {
    switch (control.GetPrescaler()) {
    case TimerPrescaler::Divide1:
        return 1;
    case TimerPrescaler::Divide64:
        return 64;
    case TimerPrescaler::Divide256:
        return 256;
    case TimerPrescaler::Divide1024:
        return 1024;
    default:
        return 1;
    }
}

void Timer::UpdateCycles(u64 cpu_cycles) {
    if (!control.IsEnabled()) return;
    
    u32 divisor = GetPrescaleDivisor();
    u32 cpu_cycles_per_tick = static_cast<u32>(divisor);
    
    cycles_elapsed += cpu_cycles;
    
    while (cycles_elapsed >= cpu_cycles_per_tick) {
        cycles_elapsed -= cpu_cycles_per_tick;
        
        if (counter > 0) {
            --counter;
        } else {
            // Counter underflow
            if (control.GetMode() == TimerMode::FreeRun) {
                counter = reload_value;
            } else {
                // OneShot mode - stop
                control.SetEnabled(false);
            }
            
            // Trigger interrupt if enabled
            if (interrupt_enable) {
                interrupt_pending = true;
                if (on_interrupt) {
                    on_interrupt(id);
                }
            }
        }
    }
}

void Timer::SetInterruptCallback(InterruptCallback callback) {
    on_interrupt = callback;
}

// ============================================================================
// Timer Manager Implementation
// ============================================================================

TimerManager* g_timer_manager = nullptr;

TimerManager::TimerManager(u64 cpu_clock_rate) : total_cpu_cycles(0) {
    // Create 4 timers
    for (u32 i = 0; i < 4; ++i) {
        timers.emplace_back(i, cpu_clock_rate);
    }
    NOTICE_LOG(HW, "TimerManager initialized with 4 timers");
}

Timer& TimerManager::GetTimer(u32 id) {
    if (id >= 4) {
        id = 0;  // Bounds check
    }
    return timers[id];
}

void TimerManager::Advance(u64 cpu_cycles) {
    total_cpu_cycles += cpu_cycles;
    
    for (auto& timer : timers) {
        timer.UpdateCycles(cpu_cycles);
    }
}

std::vector<u32> TimerManager::GetPendingInterrupts() {
    std::vector<u32> pending;
    for (u32 i = 0; i < timers.size(); ++i) {
        if (timers[i].HasInterrupt()) {
            pending.push_back(i);
        }
    }
    return pending;
}

void TimerManager::Reset() {
    for (auto& timer : timers) {
        timer.SetCounter(0);
        timer.SetReload(0);
        timer.SetControl(0);
    }
    total_cpu_cycles = 0;
}

void InitTimerManager(u64 cpu_clock_rate) {
    if (!g_timer_manager) {
        g_timer_manager = new TimerManager(cpu_clock_rate);
    }
}

void ShutdownTimerManager() {
    if (g_timer_manager) {
        delete g_timer_manager;
        g_timer_manager = nullptr;
    }
}

} // namespace Timer
} // namespace HW
