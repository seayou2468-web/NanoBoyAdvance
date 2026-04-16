// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "services.h"
#include "../../common/log.h"
#include "../../common/string_util.h"
#include <cstring>
#include <algorithm>

namespace HLE {
namespace Service {

// ============================================================================
// SM Service Implementation
// ============================================================================

void SM_Service_Extended::GetServiceHandle(u32* buffer) {
    // Get service name from request
    // IPC format: [header][cmd_id][name_bytes][name_bytes]
    
    if (!buffer) {
        WARN_LOG(KERNEL, "SM: GetServiceHandle - invalid buffer");
        return;
    }
    
    // Service name is passed in R1-R2 (little-endian, 8 bytes)
    // For now, use static service name table
    
    // Common service names (3DS)
    const std::string service_names[] = {
        "gsp::Gpu",      // Graphics
        "gsp::Ds",       // DS cartridge
        "hid:USER",      // HID input
        "ptm:u",         // Power
        "cfg:u",         // Config
        "fs:USER",       // File System
        "ps",            // Parental Controls
        "ac:u",          // Internet Connection
        "http:C",        // HTTP
        "ssl:C",         // SSL
        "frd:u",         // Friends
        "acu:u",    // ACU extra
        "act:u",         // Account
        "cecd:m",        // StreetPass
    };
    
    // In real implementation, read service name from R1-R2
    std::string requested_service = "gsp::Gpu";  // Default
    
    auto it = services.find(requested_service);
    if (it != services.end()) {
        Handle service_handle = 0x1000 + services.size();
        handle_map[service_handle] = requested_service;
        
        buffer[0] = 0x10;        // Response header
        buffer[1] = 0;           // Result code (success)
        buffer[2] = service_handle;
        
        NOTICE_LOG(KERNEL, "SM: GetServiceHandle(%s) -> 0x%x", 
                   requested_service.c_str(), service_handle);
    } else {
        buffer[0] = 0x10;
        buffer[1] = 0xD8400060;  // INVALID_HANDLE
        WARN_LOG(KERNEL, "SM: Service not found: %s", requested_service.c_str());
    }
}

void SM_Service_Extended::RegisterService(u32* buffer) {
    NOTICE_LOG(KERNEL, "SM: RegisterService");
    buffer[1] = 0;  // Success
}

void SM_Service_Extended::GetServiceNameByHandle(u32* buffer) {
    if (!buffer) return;
    
    Handle handle = buffer[0];
    auto it = handle_map.find(handle);
    if (it != handle_map.end()) {
        NOTICE_LOG(KERNEL, "SM: GetServiceNameByHandle(0x%x) -> %s", 
                   handle, it->second.c_str());
        buffer[1] = 0;
    } else {
        WARN_LOG(KERNEL, "SM: Handle not found: 0x%x", handle);
        buffer[1] = 0xD8400060;
    }
}

void SM_Service_Extended::Publish(u32* buffer) {
    NOTICE_LOG(KERNEL, "SM: Publish");
    buffer[1] = 0;
}

void SM_Service_Extended::Subscribe(u32* buffer) {
    NOTICE_LOG(KERNEL, "SM: Subscribe");
    buffer[1] = 0;
}

// ============================================================================
// GSP Service Implementation
// ============================================================================

void GSP_Service_Extended::Initialize(u32* buffer) {
    if (!buffer) return;
    
    // Note: On real 3DS, the GSP module is pre-initialized
    // PID is passed to identify the process
    
    initialized = true;
    gpu_context = 0x1;  // Context handle
    
    buffer[0] = 0x20;       // Response header
    buffer[1] = 0;          // Result code
    buffer[2] = gpu_context;
    
    NOTICE_LOG(KERNEL, "GSP: Initialize (context=0x%x)", gpu_context);
}

void GSP_Service_Extended::FramebufferInfoInit(u32* buffer) {
    if (!buffer) return;
    
    // Parameters passed in buffer
    // Usually called once at startup to configure framebuffer addresses
    
    // Top screen framebuffer: 400x240, typically at 0x1F000000 (phys)
    framebuffer_top.width = 400;
    framebuffer_top.height = 240;
    framebuffer_top.format = 0;  // RGB565
    framebuffer_top.phys_addr = 0x1F000000;
    framebuffer_top.stride = 400 * 2;  // RGB565 = 2 bytes/pixel
    framebuffer_top.status = 1;
    
    // Bottom screen: 320x240
    framebuffer_bottom.width = 320;
    framebuffer_bottom.height = 240;
    framebuffer_bottom.format = 0;
    framebuffer_bottom.phys_addr = 0x1F046500;  // Offset from top
    framebuffer_bottom.stride = 320 * 2;
    framebuffer_bottom.status = 1;
    
    buffer[0] = 0x20;
    buffer[1] = 0;  // Success
    
    NOTICE_LOG(KERNEL, "GSP: FramebufferInfoInit (top: 400x240, bottom: 320x240)");
}

void GSP_Service_Extended::CaptureScreenshot(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "GSP: CaptureScreenshot");
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void GSP_Service_Extended::DisplayTransfer(u32* buffer) {
    if (!buffer) return;
    
    // Parameters:
    // R0 = source address (GPU memory)
    // R1 = destination address (LCD)
    // R2 = transfer size
    // R3 = flags
    
    NOTICE_LOG(KERNEL, "GSP: DisplayTransfer");
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void GSP_Service_Extended::FlushFramebuffer(u32* buffer) {
    if (!buffer) return;
    
    // Triggers display update
    NOTICE_LOG(KERNEL, "GSP: FlushFramebuffer");
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void GSP_Service_Extended::GetFramebufferAddress(u32* buffer) {
    if (!buffer) return;
    
    // Returns physical addresses of framebuffers
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = framebuffer_top.phys_addr;
    buffer[3] = framebuffer_bottom.phys_addr;
    
    NOTICE_LOG(KERNEL, "GSP: GetFramebufferAddress (top=0x%x, bottom=0x%x)",
               framebuffer_top.phys_addr, framebuffer_bottom.phys_addr);
}

// ============================================================================
// PTM Service Implementation
// ============================================================================

PTM_Service_Extended::PTM_Service_Extended() {
    battery.level = 5;
    battery.is_charging = 0;
    battery.is_adapter_connected = 1;
    battery.is_shell_open = 0;
    is_new_3ds = true;  // Assume New 3DS
}

void PTM_Service_Extended::CheckNew3DS(u32* buffer) {
    if (!buffer) return;
    
    // Returns 1 if New 3DS, 0 if Old 3DS
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = is_new_3ds ? 1 : 0;
    
    NOTICE_LOG(KERNEL, "PTM: CheckNew3DS -> %d", is_new_3ds);
}

void PTM_Service_Extended::GetBatteryLevel(u32* buffer) {
    if (!buffer) return;
    
    // Returns battery level 0-5 (5 = full)
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = battery.level;
    
    NOTICE_LOG(KERNEL, "PTM: GetBatteryLevel -> %d", battery.level);
}

void PTM_Service_Extended::GetBatteryChargeState(u32* buffer) {
    if (!buffer) return;
    
    // Returns charge state (0=discharging, 1=charging, 2=full)
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = battery.is_charging ? 1 : 0;
    
    NOTICE_LOG(KERNEL, "PTM: GetBatteryChargeState -> %d", battery.is_charging);
}

void PTM_Service_Extended::GetAdapterState(u32* buffer) {
    if (!buffer) return;
    
    // Returns 1 if AC adapter is connected
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = battery.is_adapter_connected ? 1 : 0;
    
    NOTICE_LOG(KERNEL, "PTM: GetAdapterState -> %d", battery.is_adapter_connected);
}

void PTM_Service_Extended::GetShellOpenState(u32* buffer) {
    if (!buffer) return;
    
    // 3DS has a magnetic sensor to detect hinge open/close
    // Returns 1 if shell is open
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = battery.is_shell_open ? 1 : 0;
    
    NOTICE_LOG(KERNEL, "PTM: GetShellOpenState -> %d", battery.is_shell_open);
}

void PTM_Service_Extended::GetBoardPowerControlStatus(u32* buffer) {
    if (!buffer) return;
    
    // Returns power status bitfield
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = 1;  // Power on
    
    NOTICE_LOG(KERNEL, "PTM: GetBoardPowerControlStatus");
}

void PTM_Service_Extended::WakeInterrupt(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "PTM: WakeInterrupt (wake from sleep)");
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void PTM_Service_Extended::RequestSleep(u32* buffer) {
    if (!buffer) return;
    
    WARN_LOG(KERNEL, "PTM: RequestSleep (going to low-power mode)");
    buffer[0] = 0x20;
    buffer[1] = 0;
    
    // In real emulator, would put the system into low-power mode
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<SM_Service_Extended> CreateSMService() {
    return std::make_unique<SM_Service_Extended>();
}

std::unique_ptr<GSP_Service_Extended> CreateGSPService() {
    return std::make_unique<GSP_Service_Extended>();
}

std::unique_ptr<PTM_Service_Extended> CreatePTMService() {
    return std::make_unique<PTM_Service_Extended>();
}

} // namespace Service
} // namespace HLE
