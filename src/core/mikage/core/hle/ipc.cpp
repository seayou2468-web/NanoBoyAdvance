// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ipc.h"
#include "../../common/log.h"
#include <algorithm>

namespace HLE {
namespace IPC {

// ============================================================================
// CommandBuffer Implementation
// ============================================================================

CommandBuffer::CommandBuffer(u32 size) : buffer(size, 0) {}

CommandBuffer::~CommandBuffer() {}

IPCCommandHeader CommandBuffer::GetHeader() const {
    IPCCommandHeader header;
    header.raw = buffer[0];
    return header;
}

void CommandBuffer::SetHeader(IPCCommandHeader header) {
    buffer[0] = header.raw;
}

u32 CommandBuffer::GetParameter(size_t index) const {
    if (index >= buffer.size()) {
        WARN_LOG(HLE, "CommandBuffer: Parameter index out of bounds: %zu", index);
        return 0;
    }
    return buffer[index];
}

void CommandBuffer::SetParameter(size_t index, u32 value) {
    if (index >= buffer.size()) {
        WARN_LOG(HLE, "CommandBuffer: Parameter index out of bounds: %zu", index);
        return;
    }
    buffer[index] = value;
}

u64 CommandBuffer::GetParameter64(size_t index) const {
    if (index + 1 >= buffer.size()) {
        return 0;
    }
    return (static_cast<u64>(buffer[index + 1]) << 32) | buffer[index];
}

void CommandBuffer::SetParameter64(size_t index, u64 value) {
    if (index + 1 >= buffer.size()) {
        return;
    }
    buffer[index] = static_cast<u32>(value & 0xFFFFFFFF);
    buffer[index + 1] = static_cast<u32>(value >> 32);
}

void CommandBuffer::SetResultCode(u32 code) {
    if (buffer.size() >= 2) {
        buffer[1] = code;
    }
}

u32 CommandBuffer::GetResultCode() const {
    if (buffer.size() >= 2) {
        return buffer[1];
    }
    return 0;
}

// ============================================================================
// Session Implementation
// ============================================================================

Session::Session(Handle h, const std::string& name) 
    : handle(h), service_name(name) {
    NOTICE_LOG(HLE, "IPC Session created: handle=0x%x, service=%s", handle, name.c_str());
}

void Session::SendCommandBuffer(CommandBuffer& cmd_buf) {
    auto header = cmd_buf.GetHeader();
    NOTICE_LOG(HLE, "IPC Request: service=%s, command=0x%x, params=%d, static_bufs=%d",
               service_name.c_str(), header.GetCommandId(), 
               header.GetNormalParamCount(), header.GetStaticBufferCount());
}

// ============================================================================
// Server Implementation
// ============================================================================

Session* Server::AcceptConnection(const std::string& service_name) {
    static u32 next_handle = 0x1000;
    
    auto session = std::make_shared<Session>(++next_handle, service_name);
    sessions.push_back(session);
    
    NOTICE_LOG(HLE, "IPC Server: Accepted connection for service '%s' (handle=0x%x)",
               service_name.c_str(), next_handle);
    
    return session.get();
}

void Server::HandleRequest(Session* session, CommandBuffer& cmd_buf) {
    if (!session) return;
    
    NOTICE_LOG(HLE, "IPC Server: Handling request for service '%s'",
               session->GetServiceName().c_str());
    
    session->SendCommandBuffer(cmd_buf);
}

// ============================================================================
// Port Registry Implementation
// ============================================================================

void PortRegistry::RegisterPort(const std::string& name, CommandHandler handler) {
    auto port = std::make_unique<Port>(name, handler);
    ports[name] = std::move(port);
    NOTICE_LOG(HLE, "IPC Port registered: %s", name.c_str());
}

Port* PortRegistry::GetPort(const std::string& name) {
    auto it = ports.find(name);
    if (it != ports.end()) {
        return it->second.get();
    }
    WARN_LOG(HLE, "IPC Port not found: %s", name.c_str());
    return nullptr;
}

} // namespace IPC
} // namespace HLE
