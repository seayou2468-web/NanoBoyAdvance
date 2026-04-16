// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <string>
#include <map>
#include <memory>

namespace HLE {
namespace Service {

// ============================================================================
// IPC Message Structure (3DS ARM11 Format)
// ============================================================================

struct IPCMessage {
    // Header format:
    // Bits 0-15:   Command ID
    // Bits 16-19:  Static buffer count
    // Bits 20-22:  Translation parameters
    // Bits 23:     Reserved
    // Bits 24-27:  Normal params count (in u32 units)
    
    u32 header;
    u32 data_size;  // Command-specific data size
    
    // Accessors
    u32 GetCommandId() const { return (header >> 0) & 0xFFFF; }
    u32 GetStaticBufferCount() const { return (header >> 16) & 0xF; }
    u32 GetTranslationDescriptors() const { return (header >> 20) & 0x7; }
    u32 GetNormalParamCount() const { return (header >> 24) & 0xF; }
};

// ============================================================================
// Service Manager (SM) - Service Discovery & Connection
// ============================================================================

class SM_Service_Extended {
public:
    SM_Service_Extended() = default;
    
    // Get service handle by name
    // In: R0 = 0 (unused), R1-R2 = service name (8 bytes)
    // Out: R0 = result code, R1 = service handle
    void GetServiceHandle(u32* buffer);
    
    // Register service (kernel only)
    void RegisterService(u32* buffer);
    
    // Get service name by handle
    void GetServiceNameByHandle(u32* buffer);
    
    // Publish to subscribe (publish port)
    void Publish(u32* buffer);
    
    // Subscribe to service
    void Subscribe(u32* buffer);

private:
    static constexpr u32 MAX_SERVICE_NAME_LENGTH = 8;
    
    struct ServiceInfo {
        std::string name;
        Handle port_handle;
        bool is_domain;
    };
    
    std::map<std::string, ServiceInfo> services;
    std::map<Handle, std::string> handle_map;
};

// ============================================================================
// Graphics Service (GSP) - GPU & Display Control
// ============================================================================

class GSP_Service_Extended {
public:
    GSP_Service_Extended() = default;
    
    // Initialize graphics service
    // In: R0 = pid, R1 = reserved
    // Out: R0 = result code, R1 = context
    void Initialize(u32* buffer);
    
    // Set framebuffer information
    void FramebufferInfoInit(u32* buffer);
    
    // Capture framebuffer
    void CaptureScreenshot(u32* buffer);
    
    // Set display transfer
    // For DMA transfer from GPU memory to LCD
    void DisplayTransfer(u32* buffer);
    
    // Flush framebuffer (present)
    void FlushFramebuffer(u32* buffer);
    
    // Get framebuffer address
    void GetFramebufferAddress(u32* buffer);

private:
    struct FramebufferConfig {
        u32 phys_addr;     // Physical address
        u32 stride;        // Line stride
        u16 width;
        u16 height;
        u8 format;         // RGB565, RGBA8888, etc.
        u8 status;         // Active/inactive
    };
    
    FramebufferConfig framebuffer_top;
    FramebufferConfig framebuffer_bottom;
    u32 gpu_context;
    bool initialized;
};

// ============================================================================
// Power Management Service (PTM) - Battery & Power Control
// ============================================================================

class PTM_Service_Extended {
public:
    PTM_Service_Extended();
    
    // Check if CFG unit exists (for hardware check)
    void CheckNew3DS(u32* buffer);
    
    // Get battery level (0-5)
    void GetBatteryLevel(u32* buffer);
    
    // Get battery charge state (charging/discharging/full)
    void GetBatteryChargeState(u32* buffer);
    
    // Get adapter state (connected/disconnected)
    void GetAdapterState(u32* buffer);
    
    // Shell open state (lid open/closed)
    void GetShellOpenState(u32* buffer);
    
    // Get board power status
    void GetBoardPowerControlStatus(u32* buffer);
    
    // Wake interrupt (for sleep)
    void WakeInterrupt(u32* buffer);
    
    // Request sleep (low power mode)
    void RequestSleep(u32* buffer);

private:
    struct BatteryInfo {
        u8 level;           // 0-5
        u8 is_charging;     // boolean
        u8 is_adapter_connected;
        u8 is_shell_open;   // 3DS shell
    };
    
    BatteryInfo battery;
    bool is_new_3ds;
};

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<SM_Service_Extended> CreateSMService();
std::unique_ptr<GSP_Service_Extended> CreateGSPService();
std::unique_ptr<PTM_Service_Extended> CreatePTMService();

} // namespace Service
} // namespace HLE
