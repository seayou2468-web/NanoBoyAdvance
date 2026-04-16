// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "hle.h"
#include "kernel/svc.h"
#include "kernel/thread.h"
#include "service.h"
#include "../../common/log.h"
#include "../arm/interpreter/arm_interpreter.h"
#include <map>

namespace HLE {

// ============================================================================
// HLE Global State
// ============================================================================

static bool g_hle_initialized = false;

// ============================================================================
// HLE Initialization & Shutdown
// ============================================================================

void InitHLE() {
    if (g_hle_initialized) return;
    
    NOTICE_LOG(HLE, "Initializing HLE subsystems");
    
    // Initialize kernel
    Kernel::ThreadingInit();
    
    // Initialize services
    Service::InitServices();
    
    g_hle_initialized = true;
    NOTICE_LOG(HLE, "HLE subsystems initialized");
}

void ShutdownHLE() {
    if (!g_hle_initialized) return;
    
    NOTICE_LOG(HLE, "Shutting down HLE subsystems");
    
    // Shutdown services
    Service::ShutdownServices();
    
    // Shutdown kernel
    Kernel::ThreadingShutdown();
    
    g_hle_initialized = false;
    NOTICE_LOG(HLE, "HLE subsystems shutdown complete");
}

// ============================================================================
// SVC Execution
// ============================================================================

HLE::Kernel::ResultCode ExecuteSVC(u32 svc_id, void* arm_interpreter_ptr) {
    auto cpu = static_cast<ARM_Interpreter*>(arm_interpreter_ptr);
    if (!cpu) {
        WARN_LOG(HLE, "Invalid ARM interpreter pointer in ExecuteSVC");
        return HLE::Kernel::ResultCode::INVALID_ENUM_VALUE;
    }
    
    NOTICE_LOG(HLE, "Executing SVC 0x%02X", svc_id);
    
    HLE::Kernel::SVCContext ctx(*cpu);
    return HLE::Kernel::g_svc_dispatcher.Dispatch(svc_id, *cpu);
}

// ============================================================================
// Service Helpers
// ============================================================================

namespace Helpers {

std::shared_ptr<Service::ServiceInterface> GetService(const std::string& name) {
    if (!Service::g_service_manager) {
        WARN_LOG(HLE, "Service manager not initialized");
        return nullptr;
    }
    return Service::g_service_manager->GetService(name);
}

Handle ConnectService(const std::string& name) {
    if (!Service::g_service_manager) {
        WARN_LOG(HLE, "Service manager not initialized");
        return 0;
    }
    return Service::g_service_manager->ConnectToService(name);
}

HLE::Kernel::ResultCode SendServiceRequest(Handle service_handle, u32 command_id, u32* buffer) {
    // For now, return success
    return HLE::Kernel::ResultCode::SUCCESS;
}

} // namespace Helpers

} // namespace HLE
