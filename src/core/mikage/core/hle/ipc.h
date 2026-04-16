// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <vector>
#include <memory>
#include <string>

namespace HLE {
namespace IPC {

// ============================================================================
// IPC Command Buffer Format (3DS ARM11)
// ============================================================================
//
// [0] = Header (u32)
//       Bits 0-15:   Command ID (0-0xFFFF)
//       Bits 16-19:  Static buffer count (0-15 descriptors)
//       Bits 20-22:  Translation parameters flag (see below)
//       Bits 23:     Reserved
//       Bits 24-27:  Normal parameters (in u32 units)
//       Bits 28-31:  Reserved
//
// Translation parameters:
//   0: No translation
//   1: R0 contains pointer to buffer (32-bit process)
//   2: R0, R1 contain pointers (32-bit process)
//   3: R0, R1, R2 contain pointers
//   4: R0, R1, R2, R3 contain pointers
//

struct IPCCommandHeader {
    u32 raw;
    
    u32 GetCommandId() const { return (raw >> 0) & 0xFFFF; }
    u32 GetStaticBufferCount() const { return (raw >> 16) & 0xF; }
    u32 GetTranslationParamCount() const { return (raw >> 20) & 0x7; }
    u32 GetNormalParamCount() const { return (raw >> 24) & 0xF; }
    
    void SetCommandId(u32 id) { 
        raw = (raw & ~0xFFFF) | (id & 0xFFFF); 
    }
    void SetStaticBufferCount(u32 count) {
        raw = (raw & ~(0xF << 16)) | ((count & 0xF) << 16);
    }
    void SetTranslationParams(u32 params) {
        raw = (raw & ~(0x7 << 20)) | ((params & 0x7) << 20);
    }
    void SetNormalParamCount(u32 count) {
        raw = (raw & ~(0xF << 24)) | ((count & 0xF) << 24);
    }
};

// ============================================================================
// Static Buffer Descriptor (for large buffers)
// ============================================================================

struct StaticBufferDescriptor {
    u32 descriptor;
    
    // Format: [addr(28) | id(4)]
    u32 GetAddress() const { return (descriptor & 0xFFFFFFF0); }
    u32 GetId() const { return (descriptor & 0xF); }
    
    StaticBufferDescriptor(u32 addr, u32 id) 
        : descriptor((addr & 0xFFFFFFF0) | (id & 0xF)) {}
};

// ============================================================================
// IPC Command Buffer
// ============================================================================

class CommandBuffer {
public:
    explicit CommandBuffer(u32 size = 64);
    ~CommandBuffer();
    
    // Get/Set header
    IPCCommandHeader GetHeader() const;
    void SetHeader(IPCCommandHeader header);
    
    // Get/set parameter
    u32 GetParameter(size_t index) const;
    void SetParameter(size_t index, u32 value);
    
    // Get 64-bit parameter (pair of u32s)
    u64 GetParameter64(size_t index) const;
    void SetParameter64(size_t index, u64 value);
    
    // Handle buffer access
    u32* GetBuffer() { return buffer.data(); }
    const u32* GetBuffer() const { return buffer.data(); }
    size_t GetSize() const { return buffer.size(); }
    
    // Response handling
    void SetResultCode(u32 code);
    u32 GetResultCode() const;

private:
    std::vector<u32> buffer;
};

// ============================================================================
// IPC Session
// ============================================================================

class Session {
public:
    typedef u32 Handle;
    
    Session(Handle handle, const std::string& service_name);
    ~Session() = default;
    
    Handle GetHandle() const { return handle; }
    const std::string& GetServiceName() const { return service_name; }
    
    // Send IPC request
    void SendCommandBuffer(CommandBuffer& cmd_buf);
    
private:
    Handle handle;
    std::string service_name;
};

// ============================================================================
// IPC Server (Receives requests from client)
// ============================================================================

class Server {
public:
    Server() = default;
    ~Server() = default;
    
    // Accept connection
    Session* AcceptConnection(const std::string& service_name);
    
    // Handle incoming request
    void HandleRequest(Session* session, CommandBuffer& cmd_buf);

private:
    std::vector<std::shared_ptr<Session>> sessions;
};

// ============================================================================
// IPC Port Mapping (Service Name -> Port)
// ============================================================================

typedef std::function<void(CommandBuffer&)> CommandHandler;

class Port {
public:
    Port(const std::string& name, CommandHandler handler) 
        : service_name(name), command_handler(handler) {}
    
    const std::string& GetName() const { return service_name; }
    void HandleCommand(CommandBuffer& cmd_buf) {
        if (command_handler) {
            command_handler(cmd_buf);
        }
    }

private:
    std::string service_name;
    CommandHandler command_handler;
};

// ============================================================================
// Global IPC Port Registry
// ============================================================================

class PortRegistry {
public:
    static PortRegistry& Instance() {
        static PortRegistry instance;
        return instance;
    }
    
    void RegisterPort(const std::string& name, CommandHandler handler);
    Port* GetPort(const std::string& name);

private:
    PortRegistry() = default;
    std::map<std::string, std::unique_ptr<Port>> ports;
};

} // namespace IPC
} // namespace HLE
