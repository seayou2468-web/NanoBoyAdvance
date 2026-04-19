// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "dma.h"
#include "../../common/log.h"
#include <algorithm>

namespace HW {
namespace DMA {

// ============================================================================
// DMA Channel Implementation
// ============================================================================

DMAChannel::DMAChannel(u32 ch_id)
    : channel_id(ch_id), source_addr(0), dest_addr(0),
      transfer_count(0), busy(false) {
    control.raw = 0;
}

void DMAChannel::SetSourceAddress(u32 addr) {
    source_addr = addr;
    NOTICE_LOG(HW, "DMA Channel %d: Source Address = 0x%08x", channel_id, addr);
}

void DMAChannel::SetDestinationAddress(u32 addr) {
    dest_addr = addr;
    NOTICE_LOG(HW, "DMA Channel %d: Destination Address = 0x%08x", channel_id, addr);
}

void DMAChannel::SetTransferCount(u32 count) {
    transfer_count = count;
    NOTICE_LOG(HW, "DMA Channel %d: Transfer Count = %u", channel_id, count);
}

void DMAChannel::SetControl(u32 value) {
    control.raw = value;
    NOTICE_LOG(HW, "DMA Channel %d: Control = 0x%08x (mode=%d, src_mode=%d, dst_mode=%d, enabled=%d)",
               channel_id, value, 
               static_cast<u32>(control.GetTransferMode()),
               static_cast<u32>(control.GetSourceAddressMode()),
               static_cast<u32>(control.GetDestAddressMode()),
               control.IsEnabled());
    
    // If enabled, start transfer
    if (control.IsEnabled()) {
        ExecuteTransfer();
    }
}

void DMAChannel::ExecuteTransfer() {
    if (!control.IsEnabled() || transfer_count == 0) {
        return;
    }
    
    busy = true;
    
    // In real implementation, would memcpy from source to dest
    // For emulation, just log the transfer
    u32 bytes = transfer_count * 4;  // Usually 32-bit transfers
    
    NOTICE_LOG(HW, "DMA Channel %d: Transferring %u bytes from 0x%08x to 0x%08x",
               channel_id, bytes, source_addr, dest_addr);
    
    // Simulate transfer (would call actual memory copy)
    // memcpy(dest_addr, source_addr, bytes);
    
    busy = false;
    control.SetEnabled(false);  // Transfer complete
}

// ============================================================================
// DMA Controller Implementation
// ============================================================================

DMAController* g_dma_controller = nullptr;

DMAController::DMAController() : dma_enabled(true) {
    // Create 4 DMA channels
    for (u32 i = 0; i < NUM_DMA_CHANNELS; ++i) {
        channels.emplace_back(i);
    }
    NOTICE_LOG(HW, "DMA Controller initialized with %d channels", NUM_DMA_CHANNELS);
}

DMAChannel& DMAController::GetChannel(u32 id) {
    if (id >= NUM_DMA_CHANNELS) {
        id = 0;
    }
    return channels[id];
}

void DMAController::Update() {
    if (!dma_enabled) return;
    
    for (auto& channel : channels) {
        if (channel.IsBusy()) {
            channel.ExecuteTransfer();
        }
    }
}

void DMAController::RequestDMA(u32 channel_id) {
    if (channel_id >= NUM_DMA_CHANNELS) return;
    
    NOTICE_LOG(HW, "DMA Request for channel %d", channel_id);
    channels[channel_id].ExecuteTransfer();
}

void DMAController::Reset() {
    for (auto& channel : channels) {
        channel.SetSourceAddress(0);
        channel.SetDestinationAddress(0);
        channel.SetTransferCount(0);
        channel.SetControl(0);
    }
}

void InitDMAController() {
    if (!g_dma_controller) {
        g_dma_controller = new DMAController();
    }
}

void ShutdownDMAController() {
    if (g_dma_controller) {
        delete g_dma_controller;
        g_dma_controller = nullptr;
    }
}

} // namespace DMA
} // namespace HW
