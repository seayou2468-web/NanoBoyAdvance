// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "kernel.h"
#include "thread.h"
#include "../../../common/common.h"
#include "../../../common/thread_queue_list.h"
#include <memory>
#include <queue>
#include <map>
#include <algorithm>

namespace Kernel {

// ============================================================================
// ThreadManager Global Instance
// ============================================================================

class ThreadManager {
public:
    ThreadManager() : next_handle(0x1000), next_thread_id(1000), current_thread(nullptr) {
        // Initialize 64 priority queues (for priorities 0-63)
        ready_queues.resize(64);
    }
    
    std::shared_ptr<Thread> CreateThread(const char* name, u32 entry_point, s32 priority, 
                                        u32 arg, s32 processor_id, u32 stack_top);
    
    std::shared_ptr<Thread> GetThread(Handle handle);
    std::shared_ptr<Thread> GetCurrentThread() { return current_thread; }
    Handle GetCurrentThreadHandle() { 
        return current_thread ? current_thread->GetHandle() : 0; 
    }
    
    void Reschedule();
    void WakeupThread(Handle handle);
    void SuspendThread(Handle handle);
    void ExitThread(Handle handle);
    
    Handle GenerateHandle() { return ++next_handle; }
    Handle GenerateThreadId() { return ++next_thread_id; }

private:
    std::map<Handle, std::shared_ptr<Thread>> threads;
    std::shared_ptr<Thread> current_thread;
    std::vector<std::queue<std::shared_ptr<Thread>>> ready_queues;
    
    Handle next_handle;
    Handle next_thread_id;
};

// Global instance
static ThreadManager* g_thread_manager = nullptr;

void InitThreadManager() {
    if (!g_thread_manager) {
        g_thread_manager = new ThreadManager();
    }
}

void ShutdownThreadManager() {
    if (g_thread_manager) {
        delete g_thread_manager;
        g_thread_manager = nullptr;
    }
}

ThreadManager* GetThreadManager() {
    if (!g_thread_manager) InitThreadManager();
    return g_thread_manager;
}

// ============================================================================
// Wrapper Functions
// ============================================================================

Handle CreateThread(const char* name, u32 entry_point, s32 priority, u32 arg, 
                    s32 processor_id, u32 stack_top, int stack_size) {
    // Entry point is valid address
    return GetThreadManager()->CreateThread(name, entry_point, priority, arg, processor_id, stack_top);
}

Handle SetupMainThread(s32 priority, int stack_size) {
    // Default app entry point (0x8000000 in 3DS linker)
    return GetThreadManager()->CreateThread("MainThread", 0x08000000, priority, 0, 
                                           THREADPROCESSORID_0, 0x10000000);
}

void Reschedule() {
    GetThreadManager()->Reschedule();
}

void WaitCurrentThread(WaitType wait_type) {
    auto current = GetThreadManager()->GetCurrentThread();
    if (current) {
        // Thread will wait for synchronization
        GetThreadManager()->Reschedule();
    }
}

void ResumeThreadFromWait(Handle handle) {
    GetThreadManager()->WakeupThread(handle);
}

Handle GetCurrentThreadHandle() {
    return GetThreadManager()->GetCurrentThreadHandle();
}

void ThreadingInit() {
    InitThreadManager();
}

void ThreadingShutdown() {
    ShutdownThreadManager();
}

} // namespace Kernel
