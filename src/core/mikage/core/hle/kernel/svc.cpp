// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "svc.h"
#include "../../../common/log.h"
#include "../../memory.h"
#include "thread.h"
#include <map>
#include <functional>

namespace HLE {
namespace Kernel {

// ============================================================================
// SVC Implementations (Minimal)
// ============================================================================

// 0x01: ExitThread
ResultCode SVC_ExitThread(SVCContext& ctx) {
    NOTICE_LOG(MASTER_LOG, "SVC_ExitThread called");
    
    // Current thread should exit
    auto current = g_thread_manager.GetCurrentThread();
    if (current) {
        current->SetState(ThreadState::Dead);
    }
    
    return ResultCode::SUCCESS;
}

// 0x02: SleepThread
ResultCode SVC_SleepThread(SVCContext& ctx) {
    u32 nanoseconds = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_SleepThread(ns=0x%x)", nanoseconds);
    
    // For now, just reschedule
    g_thread_manager.Yield();
    
    return ResultCode::SUCCESS;
}

// 0x08: CreateMutex
ResultCode SVC_CreateMutex(SVCContext& ctx) {
    u32 initially_locked = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_CreateMutex(initially_locked=%d)", initially_locked);
    
    // Handle would be returned in R0
    // For now, return a dummy handle
    ctx.r0 = 0x00000000;
    
    return ResultCode::SUCCESS;
}

// 0x09: ReleaseMutex
ResultCode SVC_ReleaseMutex(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_ReleaseMutex(handle=0x%x)", handle);
    
    return ResultCode::SUCCESS;
}

// 0x0A: WaitSynchronization1
ResultCode SVC_WaitSynchronization1(SVCContext& ctx) {
    Handle handle = ctx.r0;
    s64 timeout = ctx.GetArg64();  // R2:R3
    NOTICE_LOG(MASTER_LOG, "SVC_WaitSynchronization1(handle=0x%x, timeout=%lld)", handle, timeout);
    
    // For now, return immediately
    ctx.r0 = 0;  // Index of signaled object
    
    return ResultCode::SUCCESS;
}

// 0x0B: WaitSynchronizationN
ResultCode SVC_WaitSynchronizationN(SVCContext& ctx) {
    u32 wait_count = ctx.r0;
    u32* wait_handles = reinterpret_cast<u32*>(ctx.r1);
    s32 wait_all = ctx.r2;
    s64 timeout = ctx.GetArg64();
    NOTICE_LOG(MASTER_LOG, "SVC_WaitSynchronizationN(count=%d, wait_all=%d)", wait_count, wait_all);
    
    // For now, return immediately
    ctx.r0 = 0;  // Index of signaled object
    ctx.r1 = 0;  // Number of signaled objects
    
    return ResultCode::SUCCESS;
}

// 0x0D: CreateEvent
ResultCode SVC_CreateEvent(SVCContext& ctx) {
    u32 reset_type = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_CreateEvent(reset_type=%d)", reset_type);
    
    // Return dummy handle
    ctx.r0 = 0x00000001;
    
    return ResultCode::SUCCESS;
}

// 0x0E: SignalEvent
ResultCode SVC_SignalEvent(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_SignalEvent(handle=0x%x)", handle);
    
    return ResultCode::SUCCESS;
}

// 0x0F: ClearEvent
ResultCode SVC_ClearEvent(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_ClearEvent(handle=0x%x)", handle);
    
    return ResultCode::SUCCESS;
}

// 0x12: GetSystemTick
ResultCode SVC_GetSystemTick(SVCContext& ctx) {
    // Return current CPU cycle count as u64 in R0:R1
    // For now, return dummy value
    ctx.SetReturnValue64(0x12345678);
    NOTICE_LOG(MASTER_LOG, "SVC_GetSystemTick");
    
    return ResultCode::SUCCESS;
}

// 0x13: ClosHandle
ResultCode SVC_CloseHandle(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_CloseHandle(handle=0x%x)", handle);
    
    return ResultCode::SUCCESS;
}

// 0x14: SVC_DuplicateHandle
ResultCode SVC_DuplicateHandle(SVCContext& ctx) {
    Handle src_handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_DuplicateHandle(src_handle=0x%x)", src_handle);
    
    // Return same handle for now
    ctx.r1 = src_handle;
    
    return ResultCode::SUCCESS;
}

// 0x1A: SendSyncRequest
ResultCode SVC_SendSyncRequest(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_SendSyncRequest(handle=0x%x)", handle);
    
    // Process IPC request (for now, skip)
    
    return ResultCode::SUCCESS;
}

// 0x1E: ControlMemory (virtual memory control)
ResultCode SVC_ControlMemory(SVCContext& ctx) {
    u32* out_addr = reinterpret_cast<u32*>(ctx.r0);
    u32 addr0 = ctx.r1;
    u32 addr1 = ctx.r2;
    u32 type = ctx.r3;
    NOTICE_LOG(MASTER_LOG, "SVC_ControlMemory(addr0=0x%x, addr1=0x%x, type=0x%x)", addr0, addr1, type);
    
    if (out_addr) {
        *out_addr = addr0;
    }
    ctx.r0 = 0;  // Return address (same as input for now)
    
    return ResultCode::SUCCESS;
}

// 0x27: CreateThread
ResultCode SVC_CreateThread(SVCContext& ctx) {
    u32 entry_point = ctx.r0;
    u32 arg = ctx.r1;
    u32 stack_top = ctx.r2;
    u32 priority = ctx.r3;
    NOTICE_LOG(MASTER_LOG, "SVC_CreateThread(entry=0x%x, arg=0x%x, stack=0x%x, priority=%d)",
               entry_point, arg, stack_top, priority);
    
    // Return dummy thread handle
    ctx.r0 = 0x00000010;  // Thread handle
    
    return ResultCode::SUCCESS;
}

// 0x28: ExitProcess
ResultCode SVC_ExitProcess(SVCContext& ctx) {
    NOTICE_LOG(MASTER_LOG, "SVC_ExitProcess");
    
    // Process should exit
    return ResultCode::SUCCESS;
}

// 0x32: GetProcessIdOfThread
ResultCode SVC_GetProcessIdOfThread(SVCContext& ctx) {
    Handle thread_handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_GetProcessIdOfThread(handle=0x%x)", thread_handle);
    
    ctx.r1 = 0x00000000;  // Process ID
    
    return ResultCode::SUCCESS;
}

// 0x33: GetThreadId
ResultCode SVC_GetThreadId(SVCContext& ctx) {
    Handle thread_handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_GetThreadId(handle=0x%x)", thread_handle);
    
    ctx.r1 = 0x00000001;  // Thread ID
    
    return ResultCode::SUCCESS;
}

// 0x3C: GetThreadPriority
ResultCode SVC_GetThreadPriority(SVCContext& ctx) {
    Handle handle = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_GetThreadPriority(handle=0x%x)", handle);
    
    ctx.r1 = 16;  // Default priority
    
    return ResultCode::SUCCESS;
}

// 0x3D: SetThreadPriority
ResultCode SVC_SetThreadPriority(SVCContext& ctx) {
    Handle handle = ctx.r0;
    u32 priority = ctx.r1;
    NOTICE_LOG(MASTER_LOG, "SVC_SetThreadPriority(handle=0x%x, priority=%d)", handle, priority);
    
    return ResultCode::SUCCESS;
}

// 0x68: ArbitrateAddress
ResultCode SVC_ArbitrateAddress(SVCContext& ctx) {
    u32 arb_type = ctx.r0;
    u32 address = ctx.r1;
    u32 value = ctx.r2;
    s64 timeout = ctx.GetArg64();
    NOTICE_LOG(MASTER_LOG, "SVC_ArbitrateAddress(type=%d, addr=0x%x, value=%d)", arb_type, address, value);
    
    return ResultCode::SUCCESS;
}

// 0xFF: Break (Debug break)
ResultCode SVC_Break(SVCContext& ctx) {
    u32 reason = ctx.r0;
    NOTICE_LOG(MASTER_LOG, "SVC_Break(reason=0x%x)", reason);
    
    // For now, just log and continue
    return ResultCode::SUCCESS;
}

// ============================================================================
// SVC Dispatcher Implementation
// ============================================================================

SVCDispatcher::SVCDispatcher() {
    RegisterDefaultHandlers();
}

void SVCDispatcher::RegisterDefaultHandlers() {
    // Register all SVCs
    svc_handlers[0x01] = [](SVCContext& ctx) { return SVC_ExitThread(ctx); };
    svc_handlers[0x02] = [](SVCContext& ctx) { return SVC_SleepThread(ctx); };
    svc_handlers[0x08] = [](SVCContext& ctx) { return SVC_CreateMutex(ctx); };
    svc_handlers[0x09] = [](SVCContext& ctx) { return SVC_ReleaseMutex(ctx); };
    svc_handlers[0x0A] = [](SVCContext& ctx) { return SVC_WaitSynchronization1(ctx); };
    svc_handlers[0x0B] = [](SVCContext& ctx) { return SVC_WaitSynchronizationN(ctx); };
    svc_handlers[0x0D] = [](SVCContext& ctx) { return SVC_CreateEvent(ctx); };
    svc_handlers[0x0E] = [](SVCContext& ctx) { return SVC_SignalEvent(ctx); };
    svc_handlers[0x0F] = [](SVCContext& ctx) { return SVC_ClearEvent(ctx); };
    svc_handlers[0x12] = [](SVCContext& ctx) { return SVC_GetSystemTick(ctx); };
    svc_handlers[0x13] = [](SVCContext& ctx) { return SVC_CloseHandle(ctx); };
    svc_handlers[0x14] = [](SVCContext& ctx) { return SVC_DuplicateHandle(ctx); };
    svc_handlers[0x1A] = [](SVCContext& ctx) { return SVC_SendSyncRequest(ctx); };
    svc_handlers[0x1E] = [](SVCContext& ctx) { return SVC_ControlMemory(ctx); };
    svc_handlers[0x27] = [](SVCContext& ctx) { return SVC_CreateThread(ctx); };
    svc_handlers[0x28] = [](SVCContext& ctx) { return SVC_ExitProcess(ctx); };
    svc_handlers[0x32] = [](SVCContext& ctx) { return SVC_GetProcessIdOfThread(ctx); };
    svc_handlers[0x33] = [](SVCContext& ctx) { return SVC_GetThreadId(ctx); };
    svc_handlers[0x3C] = [](SVCContext& ctx) { return SVC_GetThreadPriority(ctx); };
    svc_handlers[0x3D] = [](SVCContext& ctx) { return SVC_SetThreadPriority(ctx); };
    svc_handlers[0x68] = [](SVCContext& ctx) { return SVC_ArbitrateAddress(ctx); };
    svc_handlers[0xFF] = [](SVCContext& ctx) { return SVC_Break(ctx); };
}

ResultCode SVCDispatcher::Dispatch(u32 svc_id, ARM_Interpreter& cpu) {
    auto it = svc_handlers.find(svc_id);
    if (it == svc_handlers.end()) {
        WARN_LOG(MASTER_LOG, "Unknown SVC: 0x%02X", svc_id);
        return ResultCode::INVALID_ENUM_VALUE;
    }
    
    SVCContext ctx(cpu);
    return it->second(ctx);
}

void SVCDispatcher::RegisterSVC(u32 svc_id, SVCHandler handler) {
    svc_handlers[svc_id] = handler;
}

// Global SVC dispatcher
SVCDispatcher g_svc_dispatcher;

} // namespace Kernel
} // namespace HLE
