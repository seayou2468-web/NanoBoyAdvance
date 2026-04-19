// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <functional>
#include <vector>

namespace HW {
namespace DMA {

// ============================================================================
// DMA Configuration
// ============================================================================

// There are 4 DMA channels
constexpr u32 NUM_DMA_CHANNELS = 4;

enum class DMATransferMode {
    Demand = 0,         // Transfer on demand
    Block = 1,          // Transfer in blocks
    Burst = 2,          // Transfer in bursts
    SingleTransfer = 3, // Single 32-bit transfer per request
};

enum class DMAAddressMode {
    Increment = 0,       // Address increments
    Decrement = 1,       // Address decrements
    Fixed = 2,           // Address stays fixed
};

// ============================================================================
// DMA Control Register
// ============================================================================

struct DMAControl {
    u32 raw;
    
    DMATransferMode GetTransferMode() const {
        return static_cast<DMATransferMode>((raw >> 21) & 0x3);
    }
    
    DMAAddressMode GetSourceAddressMode() const {
        return static_cast<DMAAddressMode>((raw >> 23) & 0x3);
    }
    
    DMAAddressMode GetDestAddressMode() const {
        return static_cast<DMAAddressMode>((raw >> 25) & 0x3);
    }
    
    bool IsEnabled() const {
        return (raw >> 31) & 0x1;
    }
    
    void SetEnabled(bool enabled) {
        raw = (raw & ~(1u << 31)) | ((enabled ? 1 : 0) << 31);
    }
};

// ============================================================================
// DMA Channel
// ============================================================================

class DMAChannel {
public:
    DMAChannel(u32 channel_id);
    ~DMAChannel() = default;
    
    // Register access
    u32 GetSourceAddress() const { return source_addr; }
    void SetSourceAddress(u32 addr);
    
    u32 GetDestinationAddress() const { return dest_addr; }
    void SetDestinationAddress(u32 addr);
    
    u32 GetTransferCount() const { return transfer_count; }
    void SetTransferCount(u32 count);
    
    u32 GetControl() const { return control.raw; }
    void SetControl(u32 value);
    
    // Execute transfer
    void ExecuteTransfer();
    
    // Check if channel is busy
    bool IsBusy() const { return busy; }

private:
    u32 channel_id;
    u32 source_addr;
    u32 dest_addr;
    u32 transfer_count;
    DMAControl control;
    bool busy;
};

// ============================================================================
// DMA Controller
// ============================================================================

class DMAController {
public:
    DMAController();
    ~DMAController() = default;
    
    // Channel access
    DMAChannel& GetChannel(u32 id);
    
    // Global enable/disable
    bool IsEnabled() const { return dma_enabled; }
    void SetEnabled(bool enabled) { dma_enabled = enabled; }
    
    // Update all channels
    void Update();
    
    // DMA request (from peripheral)
    void RequestDMA(u32 channel_id);
    
    // Reset all channels
    void Reset();

private:
    std::vector<DMAChannel> channels;
    bool dma_enabled;
};

// ============================================================================
// Global DMA Manager
// ============================================================================

extern DMAController* g_dma_controller;

void InitDMAController();
void ShutdownDMAController();

} // namespace DMA
} // namespace HW
