// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version

#include "service.h"
#include "../../common/log.h"
#include <map>

namespace HLE {
namespace Service {

// ============================================================================
// Service Manager Implementation
// ============================================================================

ServiceManager* g_service_manager = nullptr;

ServiceManager* GetServiceManager() {
    if (!g_service_manager) {
        g_service_manager = new ServiceManager();
    }
    return g_service_manager;
}

void ServiceManager::RegisterService(const std::string& name, std::shared_ptr<ServiceInterface> service) {
    services[name] = service;
    NOTICE_LOG(KERNEL, "Registered service: %s", name.c_str());
}

std::shared_ptr<ServiceInterface> ServiceManager::GetService(const std::string& name) {
    auto it = services.find(name);
    if (it != services.end()) {
        return it->second;
    }
    WARN_LOG(KERNEL, "Service not found: %s", name.c_str());
    return nullptr;
}

Handle ServiceManager::ConnectToService(const std::string& name) {
    auto service = GetService(name);
    if (!service) {
        WARN_LOG(KERNEL, "Cannot connect to service: %s", name.c_str());
        return INVALID_HANDLE;
    }
    
    Handle handle = ++next_handle;
    service_handles[handle] = name;
    NOTICE_LOG(KERNEL, "Connected to service: %s (handle=0x%x)", name.c_str(), handle);
    return handle;
}

void ServiceManager::CloseService(Handle handle) {
    service_handles.erase(handle);
}

// ============================================================================
// Base Service Implementation
// ============================================================================

void Service::HandleSyncRequest(u32 command_header, u32* cmd_buffer, size_t buffer_size) {
    IPCHeader header;
    header.raw = command_header;
    
    NOTICE_LOG(KERNEL, "Service %s received command: 0x%04x", 
               service_name.c_str(), header.fields.command_id);
    
    HandleCommand(header.fields.command_id, cmd_buffer, buffer_size);
}

void Service::RegisterCommand(u32 command_id, CommandHandler handler) {
    command_handlers[command_id] = handler;
}

// ============================================================================
// SM Service Implementation
// ============================================================================

void SM_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "SM command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0001: {  // Initialize
        NOTICE_LOG(KERNEL, "SM: Initialize");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x20010;
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0002: {  // GetServiceHandle
        NOTICE_LOG(KERNEL, "SM: GetServiceHandle");
        // In real implementation, would parse service name from buffer
        // For now, return dummy handle
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x10;  // Dummy handle
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0003: {  // RegisterService
        NOTICE_LOG(KERNEL, "SM: RegisterService");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x20010;
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown SM command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);  // Error
        }
        break;
    }
}

// ============================================================================
// GSP Service Implementation (Graphics)
// ============================================================================

void GSP_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "GSP command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0001: {  // Initialize
        NOTICE_LOG(KERNEL, "GSP: Initialize");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x10;  // GSP context
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0002: {  // FramebufferInfoInit
        NOTICE_LOG(KERNEL, "GSP: FramebufferInfoInit");
        if (buffer_size >= 4) {
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown GSP command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);
        }
        break;
    }
}

// ============================================================================
// FS Service Implementation (File System)
// ============================================================================

void FS_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "FS command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0001: {  // Initialize
        NOTICE_LOG(KERNEL, "FS: Initialize");
        if (buffer_size >= 4) {
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0002: {  // OpenFile
        NOTICE_LOG(KERNEL, "FS: OpenFile");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x20;  // File handle
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0003: {  // ReadFile
        NOTICE_LOG(KERNEL, "FS: ReadFile");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x10;  // Bytes read
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown FS command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);
        }
        break;
    }
}

// ============================================================================
// HID Service Implementation (Input)
// ============================================================================

void HID_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "HID command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0001: {  // Initialize
        NOTICE_LOG(KERNEL, "HID: Initialize");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x1;
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x000B: {  // GetKeysHeld
        NOTICE_LOG(KERNEL, "HID: GetKeysHeld");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0x0;  // No keys pressed
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown HID command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);
        }
        break;
    }
}

// ============================================================================
// PTM Service Implementation (Power/Battery)
// ============================================================================

void PTM_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "PTM command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0000: {  // Initialize
        NOTICE_LOG(KERNEL, "PTM: Initialize");
        if (buffer_size >= 4) {
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0005: {  // GetBatteryLevel
        NOTICE_LOG(KERNEL, "PTM: GetBatteryLevel");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 5;  // Battery level (0-5)
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown PTM command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);
        }
        break;
    }
}

// ============================================================================
// CFG Service Implementation (Configuration)
// ============================================================================

void CFG_Service::HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
    NOTICE_LOG(KERNEL, "CFG command: 0x%x", command_id);
    
    switch (command_id) {
    case 0x0000: {  // GetSystemModel
        NOTICE_LOG(KERNEL, "CFG: GetSystemModel");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 4;  // New 3DS
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    case 0x0001: {  // GetModelNintendo2DS
        NOTICE_LOG(KERNEL, "CFG: GetModelNintendo2DS");
        if (buffer_size >= 4) {
            cmd_buffer[0] = 0;  // Not 2DS
            cmd_buffer[1] = ResultSuccess();
        }
        break;
    }
    default:
        WARN_LOG(KERNEL, "Unknown CFG command: 0x%x", command_id);
        if (buffer_size >= 4) {
            cmd_buffer[1] = MakeResult(1, 0, 0, 0);
        }
        break;
    }
}

// ============================================================================
// Service Initialization
// ============================================================================

void InitServices() {
    auto manager = GetServiceManager();
    
    // Register all core services
    manager->RegisterService("sm:", std::make_shared<SM_Service>());
    manager->RegisterService("gsp::Gpu", std::make_shared<GSP_Service>());
    manager->RegisterService("fs:USER", std::make_shared<FS_Service>());
    manager->RegisterService("hid:USER", std::make_shared<HID_Service>());
    manager->RegisterService("ptm:sysm", std::make_shared<PTM_Service>());
    manager->RegisterService("cfg:u", std::make_shared<CFG_Service>());
    
    NOTICE_LOG(KERNEL, "HLE Services initialized");
}

void ShutdownServices() {
    if (g_service_manager) {
        delete g_service_manager;
        g_service_manager = nullptr;
    }
    NOTICE_LOG(KERNEL, "HLE Services shutdown");
}

} // namespace Service
} // namespace HLE
