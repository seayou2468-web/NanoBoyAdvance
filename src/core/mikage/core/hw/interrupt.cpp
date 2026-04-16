// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "interrupt.h"
#include "../../common/log.h"
#include <algorithm>

namespace HW {
namespace InterruptController {

// ============================================================================
// IRQ Controller Implementation
// ============================================================================

IRQController::IRQController() 
    : interrupt_enable(0), interrupt_status(0) {
}

void IRQController::ClearInterrupt(InterruptType type) {
    u32 mask = 1u << static_cast<u32>(type);
    interrupt_status &= ~mask;
    NOTICE_LOG(HW, "IRQ: Cleared interrupt %d", static_cast<u32>(type));
}

void IRQController::RaiseInterrupt(InterruptType type) {
    u32 mask = 1u << static_cast<u32>(type);
    
    // Check if this interrupt is enabled
    if (!(interrupt_enable & mask)) {
        NOTICE_LOG(HW, "IRQ: Interrupt %d raised but not enabled", static_cast<u32>(type));
        return;
    }
    
    // Set interrupt status bit
    interrupt_status |= mask;
    
    NOTICE_LOG(HW, "IRQ: Raised interrupt %d (status=0x%08x)", 
               static_cast<u32>(type), interrupt_status);
    
    // Call callback
    if (on_interrupt) {
        on_interrupt(type);
    }
}

InterruptType IRQController::GetHighestPriority() const {
    // Priority: 0 (Timer0) > 31 (lowest)
    for (u32 i = 0; i < 32; ++i) {
        if (interrupt_status & (1u << i)) {
            return static_cast<InterruptType>(i);
        }
    }
    return InterruptType::Timer0;  // Default
}

void IRQController::SetInterruptCallback(InterruptCallback callback) {
    on_interrupt = callback;
}

// ============================================================================
// FIQ Controller Implementation
// ============================================================================

FIQController::FIQController() : status(0) {
}

void FIQController::RaiseFIQ(u32 fiq_id) {
    status |= (1u << fiq_id);
    NOTICE_LOG(HW, "FIQ: Raised FIQ %d", fiq_id);
}

void FIQController::ClearFIQ(u32 fiq_id) {
    status &= ~(1u << fiq_id);
    NOTICE_LOG(HW, "FIQ: Cleared FIQ %d", fiq_id);
}

// ============================================================================
// Interrupt Manager Implementation
// ============================================================================

InterruptManager* g_interrupt_manager = nullptr;

InterruptManager::InterruptManager() {
    NOTICE_LOG(HW, "InterruptManager initialized");
}

void InterruptManager::RaiseInterrupt(InterruptType type) {
    irq_controller.RaiseInterrupt(type);
    pending_queue.push(type);
}

std::queue<InterruptType> InterruptManager::GetPendingInterrupts() {
    auto result = pending_queue;
    // Clear the original queue (or only return, don't need to clear here)
    return result;
}

bool InterruptManager::ShouldProcessInterrupt() const {
    return (irq_controller.GetInterruptStatus() & irq_controller.GetInterruptEnable()) != 0;
}

void InterruptManager::Reset() {
    irq_controller.SetInterruptEnable(0);
    irq_controller.SetInterruptStatus(0);
    fiq_controller.SetStatus(0);
    
    // Clear pending queue
    while (!pending_queue.empty()) {
        pending_queue.pop();
    }
}

void InitInterruptManager() {
    if (!g_interrupt_manager) {
        g_interrupt_manager = new InterruptManager();
    }
}

void ShutdownInterruptManager() {
    if (g_interrupt_manager) {
        delete g_interrupt_manager;
        g_interrupt_manager = nullptr;
    }
}

} // namespace InterruptController
} // namespace HW
