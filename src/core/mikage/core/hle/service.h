// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

#include "../../common/common_types.h"
#include <string>
#include <memory>
#include <map>
#include <functional>

namespace HLE {
namespace Service {

// ============================================================================
// Service Framework
// ============================================================================

typedef u32 Handle;
typedef std::function<void(void*)> ServiceDeleter;

// IPC command headers
struct IPCHeader {
    union {
        u32 raw;
        struct {
            u32 command_id : 16;
            u32 static_buffers : 4;
            u32 translation : 3;
            u32 : 1;
            u32 normal_params : 4;
            u32 : 4;
        } fields;
    };
};

class ServiceInterface {
public:
    virtual ~ServiceInterface() = default;
    virtual const std::string& GetServiceName() const = 0;
    virtual void HandleSyncRequest(u32 command_header, u32* cmd_buffer, size_t buffer_size) = 0;
};

// Service Manager
class ServiceManager {
public:
    ServiceManager() = default;
    ~ServiceManager() = default;
    
    void RegisterService(const std::string& name, std::shared_ptr<ServiceInterface> service);
    std::shared_ptr<ServiceInterface> GetService(const std::string& name);
    
    Handle ConnectToService(const std::string& name);
    void CloseService(Handle handle);

private:
    std::map<std::string, std::shared_ptr<ServiceInterface>> services;
    std::map<Handle, std::string> service_handles;
    Handle next_handle = 0x1000;
};

extern ServiceManager* g_service_manager;

// ============================================================================
// Base Service Class
// ============================================================================

class Service : public ServiceInterface {
public:
    explicit Service(const std::string& name) : service_name(name) {}
    ~Service() override = default;
    
    const std::string& GetServiceName() const override { return service_name; }
    void HandleSyncRequest(u32 command_header, u32* cmd_buffer, size_t buffer_size) override;

protected:
    using CommandHandler = std::function<void(u32*, size_t)>;
    
    void RegisterCommand(u32 command_id, CommandHandler handler);
    virtual void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) {
        auto it = command_handlers.find(command_id);
        if (it != command_handlers.end()) {
            it->second(cmd_buffer, buffer_size);
        }
    }
    
    u32 MakeResult(u32 level, u32 summary, u32 module, u32 description) {
        return (level << 31) | (summary << 27) | (module << 20) | description;
    }
    
    u32 ResultSuccess() { return MakeResult(0, 0, 0, 0); }

private:
    std::string service_name;
    std::map<u32, CommandHandler> command_handlers;
};

// ============================================================================
// Core Service Declarations
// ============================================================================

// Service Manager (SM)
class SM_Service : public Service {
public:
    SM_Service() : Service("sm:") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// Graphics Service (GSP)
class GSP_Service : public Service {
public:
    GSP_Service() : Service("gsp::Gpu") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// File System (FS)
class FS_Service : public Service {
public:
    FS_Service() : Service("fs:USER") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// HID (Input)
class HID_Service : public Service {
public:
    HID_Service() : Service("hid:USER") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// PTM (Power/Battery)
class PTM_Service : public Service {
public:
    PTM_Service() : Service("ptm:sysm") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// CFG (Config)
class CFG_Service : public Service {
public:
    CFG_Service() : Service("cfg:u") {}
    void HandleCommand(u32 command_id, u32* cmd_buffer, size_t buffer_size) override;
};

// Initialize all services
void InitServices();
void ShutdownServices();

} // namespace Service
} // namespace HLE
