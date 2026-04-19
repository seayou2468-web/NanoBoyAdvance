// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <vector>
#include <functional>
#include <queue>

namespace HW {
namespace InterruptController {

// ============================================================================
// Interrupt Type (3DS)
// ============================================================================

enum class InterruptType : u32 {
    // Timer interrupts
    Timer0 = 0,
    Timer1 = 1,
    Timer2 = 2,
    Timer3 = 3,
    
    // LCD interrupts
    V_Blank = 4,
    H_Blank = 5,
    
    // DMA interrupts
    DMA0 = 6,
    DMA1 = 7,
    DMA2 = 8,
    
    // Miscellaneous
    PSP_Interrupt = 9,
    Cartridge = 10,
    
    // Touchscreen
    TouchScreen = 11,
    
    Total = 32,
};

// ============================================================================
// Interrupt Request (IRQ) Controller
// ============================================================================

class IRQController {
public:
    IRQController();
    ~IRQController() = default;
    
    // Get/set interrupt enable status
    u32 GetInterruptEnable() const { return interrupt_enable; }
    void SetInterruptEnable(u32 mask) { interrupt_enable = mask; }
    
    // Get/set interrupt status (pending)
    u32 GetInterruptStatus() const { return interrupt_status; }
    void SetInterruptStatus(u32 mask) { interrupt_status = mask; }
    
    // Clear interrupt (write 1 to clear)
    void ClearInterrupt(InterruptType type);
    
    // Request interrupt (from peripheral)
    void RaiseInterrupt(InterruptType type);
    
    // Check if any interrupt is pending
    bool HasPendingInterrupt() const { return interrupt_status != 0; }
    
    // Get the highest priority pending interrupt
    InterruptType GetHighestPriority() const;
    
    // Callback on interrupt
    typedef std::function<void(InterruptType)> InterruptCallback;
    void SetInterruptCallback(InterruptCallback callback);

private:
    u32 interrupt_enable;   // Bit mask of enabled interrupts
    u32 interrupt_status;   // Bit mask of pending interrupts
    
    InterruptCallback on_interrupt;
};

// ============================================================================
// Fast Interrupt Request (FIQ) Controller
// ============================================================================

class FIQController {
public:
    FIQController();
    ~FIQController() = default;
    
    // Get/set FIQ status
    u32 GetStatus() const { return status; }
    void SetStatus(u32 value) { status = value; }
    
    // Check specific FIQ
    bool IsFIQPending(u32 fiq_id) const { return (status >> fiq_id) & 0x1; }
    
    // Raise FIQ
    void RaiseFIQ(u32 fiq_id);
    
    // Clear FIQ
    void ClearFIQ(u32 fiq_id);

private:
    u32 status;
};

// ============================================================================
// Interrupt Manager (Master)
// ============================================================================

class InterruptManager {
public:
    InterruptManager();
    ~InterruptManager() = default;
    
    // Access controllers
    IRQController& GetIRQController() { return irq_controller; }
    FIQController& GetFIQController() { return fiq_controller; }
    
    // Raise interrupt (from peripherals)
    void RaiseInterrupt(InterruptType type);
    
    // Get pending interrupt queue
    std::queue<InterruptType> GetPendingInterrupts();
    
    // Check if CPU should handle interrupt
    bool ShouldProcessInterrupt() const;
    
    // Reset all interrupts
    void Reset();

private:
    IRQController irq_controller;
    FIQController fiq_controller;
    
    std::queue<InterruptType> pending_queue;
};

// ============================================================================
// Global Interrupt Manager
// ============================================================================

extern InterruptManager* g_interrupt_manager;

void InitInterruptManager();
void ShutdownInterruptManager();

} // namespace InterruptController
} // namespace HW
