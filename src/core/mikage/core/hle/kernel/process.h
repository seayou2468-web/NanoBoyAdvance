// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include "../hle/svc.h"
#include "memory.h"
#include <string>
#include <memory>
#include <vector>

namespace HLE {
namespace Kernel {

// ============================================================================
// Process Management
// ============================================================================

// Page access levels (for page tables)
enum class PageAccessLevel : u32 {
    ReadOnly = 0,
    ReadWrite = 1,
    NoAccess = 2
};

class Process {
public:
    Process();
    ~Process();
    
    // Process properties
    u32 GetID() const { return process_id_; }
    const std::string& GetName() const { return process_name_; }
    
    // Memory management
    u32 GetCodeMemoryStart() const { return code_memory_start_; }
    u32 GetCodeMemorySize() const { return code_memory_size_; }
    void SetupMemory(u32 code_start, u32 code_size, u32 heap_start, u32 heap_size);
    
    // Thread management
    void AddThread(std::shared_ptr<class Thread> thread);
    void RemoveThread(std::shared_ptr<class Thread> thread);
    const std::vector<std::shared_ptr<class Thread>>& GetThreads() const { return threads_; }
    
    // Handle table
    Handle CreateHandle(std::shared_ptr<class WaitObject> object);
    void CloseHandle(Handle handle);
    std::shared_ptr<class WaitObject> GetHandleObject(Handle handle);
    
private:
    u32 process_id_;
    std::string process_name_;
    u32 code_memory_start_;
    u32 code_memory_size_;
    u32 heap_memory_start_;
    u32 heap_memory_size_;
    
    std::vector<std::shared_ptr<class Thread>> threads_;
    std::map<Handle, std::shared_ptr<class WaitObject>> handle_table_;
};

// ============================================================================
// Process Manager
// ============================================================================

class ProcessManager {
public:
    ProcessManager();
    
    std::shared_ptr<Process> CreateProcess(const std::string& name);
    void DestroyProcess(u32 process_id);
    std::shared_ptr<Process> GetProcess(u32 process_id);
    std::shared_ptr<Process> GetCurrentProcess() { return current_process_; }
    
private:
    std::vector<std::shared_ptr<Process>> processes_;
    std::shared_ptr<Process> current_process_;
    u32 next_process_id_;
};

// Global process manager
extern ProcessManager g_process_manager;

} // namespace Kernel
} // namespace HLE
