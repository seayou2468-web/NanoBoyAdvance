// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include "../arm/interpreter/arm_interpreter.h"
#include <map>
#include <functional>

namespace HLE {
namespace Kernel {

// ============================================================================
// SVC (System Call) Interface
// ============================================================================

// SVC result codes (Citra compatible)
enum class ResultCode : u32 {
    SUCCESS = 0,
    INVALID_HANDLE = 0xD8400060,
    SESSION_CLOSED = 0xD8400001,
    INVALID_MEMORY_PERMISSIONS = 0xD8400026,
    INVALID_MEMORY_RANGE = 0xD8400025,
    INVALID_ENUM_VALUE = 0xD8400012,
    INVALID_COMBINATION = 0xD8400027,
    OUT_OF_MEMORY = 0xD8400028,
    NO_SYNCHRONIZATION_PRIMITIVE = 0xD8400001,
    TERMINATED_BY_CANCEL = 0xD8400058,
    THREAD_TERMINATION_REQUESTED = 0xD8400201,
};

// Forward declarations
class Thread;
class Process;
class Mutex;
class Event;
class Semaphore;
class Timer;

// ============================================================================
// SVC Context (Register mapping)
// ============================================================================

struct SVCContext {
    // ARM calling convention: arguments in R0-R3, result in R0
    u32& r0;  // Argument 0 / Return value
    u32& r1;  // Argument 1 / Return value (pairs for 64-bit)
    u32& r2;  // Argument 2
    u32& r3;  // Argument 3
    
    ARM_Interpreter& cpu;
    
    SVCContext(ARM_Interpreter& c) : 
        r0(c.GetReg(0)), r1(c.GetReg(1)), r2(c.GetReg(2)), r3(c.GetReg(3)), cpu(c) {}
    
    // Helper: Set 64-bit return value (in R0:R1)
    void SetReturnValue64(u64 value) {
        r0 = static_cast<u32>(value & 0xFFFFFFFFUL);
        r1 = static_cast<u32>(value >> 32);
    }
    
    // Helper: Get 64-bit argument (from R2:R3)
    u64 GetArg64() const {
        return (static_cast<u64>(r3) << 32) | r2;
    }
};

// ============================================================================
// Core SVC Functions (Subset for basic 3DS emulation)
// ============================================================================

// Thread Management
ResultCode SVC_CreateThread(SVCContext& ctx);
ResultCode SVC_ExitThread(SVCContext& ctx);
ResultCode SVC_SleepThread(SVCContext& ctx);
ResultCode SVC_GetThreadPriority(SVCContext& ctx);
ResultCode SVC_SetThreadPriority(SVCContext& ctx);
ResultCode SVC_SignalEvent(SVCContext& ctx);
ResultCode SVC_ClearEvent(SVCContext& ctx);

// Mutex
ResultCode SVC_CreateMutex(SVCContext& ctx);
ResultCode SVC_ReleaseMutex(SVCContext& ctx);
ResultCode SVC_WaitSynchronization1(SVCContext& ctx);
ResultCode SVC_WaitSynchronizationN(SVCContext& ctx);
ResultCode SVC_ArbitrateAddress(SVCContext& ctx);

// Events & Semaphores
ResultCode SVC_CreateEvent(SVCContext& ctx);
ResultCode SVC_CreateSemaphore(SVCContext& ctx);
ResultCode SVC_ReleaseSemaphore(SVCContext& ctx);
ResultCode SVC_SignalSemaphore(SVCContext& ctx);
ResultCode SVC_CancelTimer(SVCContext& ctx);
ResultCode SVC_SetTimer(SVCContext& ctx);
ResultCode SVC_GetSystemTick(SVCContext& ctx);

// Memory
ResultCode SVC_ControlMemory(SVCContext& ctx);
ResultCode SVC_QueryMemory(SVCContext& ctx);

// Process/Handler
ResultCode SVC_GetProcessIdOfThread(SVCContext& ctx);
ResultCode SVC_GetThreadId(SVCContext& ctx);
ResultCode SVC_CreateHandle(SVCContext& ctx);
ResultCode SVC_DuplicateHandle(SVCContext& ctx);
ResultCode SVC_CloseHandle(SVCContext& ctx);

// Message/IPC
ResultCode SVC_SendSyncRequest(SVCContext& ctx);
ResultCode SVC_ReplyAndReceive(SVCContext& ctx);
ResultCode SVC_CreatePort(SVCContext& ctx);
ResultCode SVC_ConnectToPort(SVCContext& ctx);
ResultCode SVC_CreateSession(SVCContext& ctx);
ResultCode SVC_AcceptSession(SVCContext& ctx);

// Debug
ResultCode SVC_Break(SVCContext& ctx);
ResultCode SVC_OutputDebugString(SVCContext& ctx);

// ============================================================================
// SVC Dispatcher
// ============================================================================

class SVCDispatcher {
public:
    SVCDispatcher();
    
    // Dispatch an SVC (System Call) request
    // SVC number is in R12 (IP) or passed as parameter
    ResultCode Dispatch(u32 svc_id, ARM_Interpreter& cpu);
    
    // Register a custom SVC handler
    using SVCHandler = std::function<ResultCode(SVCContext&)>;
    void RegisterSVC(u32 svc_id, SVCHandler handler);
    
private:
    std::map<u32, SVCHandler> svc_handlers;
    
    void RegisterDefaultHandlers();
};

// Global SVC dispatcher instance
extern SVCDispatcher g_svc_dispatcher;

// Convenience function to dispatch SVC from CPU
inline ResultCode DispatchSVC(u32 svc_id, ARM_Interpreter& cpu) {
    return g_svc_dispatcher.Dispatch(svc_id, cpu);
}

} // namespace Kernel
} // namespace HLE
