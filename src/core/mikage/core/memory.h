#pragma once

#include "./mem_map.h"
#include "../common/common_types.h"
#include <array>
#include <map>
#include <optional>
#include <vector>
#include <cstring>

namespace Memory {

// ============================================================================
// Memory Protection / Access Control
// ============================================================================

// Memory page permissions
enum class MemoryPermission : u32 {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    All = Read | Write | Execute
};

// Memory page type
enum class MemoryType : u8 {
    Unmapped = 0,
    RAM = 1,
    VRAM = 2,
    SystemRam = 3,
    DspRam = 4,
    AXWram = 5
};

// ============================================================================
// Virtual Memory Manager
// ============================================================================

class VirtualMemoryManager {
public:
    static constexpr u32 PAGE_SIZE = 0x1000;  // 4KB pages
    static constexpr u32 PAGE_MASK = PAGE_SIZE - 1;
    static constexpr u32 PAGE_SHIFT = 12;
    
    struct MemoryPage {
        void* host_ptr;
        MemoryType type;
        MemoryPermission perm;
        u8 cache_type;  // 0=WriteThrough, 1=WriteBack, 2=Non-cached
        bool is_locked;
    };
    
    VirtualMemoryManager() : page_table(0x100000) {  // 4GB / 4KB = 1M pages
        page_table.fill(MemoryPage{nullptr, MemoryType::Unmapped, MemoryPermission::None, 0, false});
    }
    
    // Map a virtual address to physical memory
    void MapMemory(u32 virt_addr, u32 phys_addr, u32 size, 
                   MemoryType type, MemoryPermission perm) {
        for (u32 offset = 0; offset < size; offset += PAGE_SIZE) {
            u32 page_idx = (virt_addr + offset) >> PAGE_SHIFT;
            if (page_idx >= page_table.size()) break;
            
            void* host_ptr = GetPhysicalPointer(phys_addr + offset);
            page_table[page_idx] = {host_ptr, type, perm, 0, false};
        }
    }
    
    // Unmap virtual memory
    void UnmapMemory(u32 virt_addr, u32 size) {
        for (u32 offset = 0; offset < size; offset += PAGE_SIZE) {
            u32 page_idx = (virt_addr + offset) >> PAGE_SHIFT;
            if (page_idx >= page_table.size()) break;
            page_table[page_idx] = {nullptr, MemoryType::Unmapped, MemoryPermission::None, 0, false};
        }
    }
    
    // Get page access permissions
    std::optional<MemoryPermission> GetPagePermission(u32 virt_addr) const {
        u32 page_idx = virt_addr >> PAGE_SHIFT;
        if (page_idx >= page_table.size()) return std::nullopt;
        
        const auto& page = page_table[page_idx];
        if (page.type == MemoryType::Unmapped) return std::nullopt;
        return page.perm;
    }
    
    // Check if address is accessible
    bool IsAccessible(u32 virt_addr, MemoryPermission required_perm) const {
        auto perm = GetPagePermission(virt_addr);
        if (!perm.has_value()) return false;
        return (static_cast<u32>(*perm) & static_cast<u32>(required_perm)) != 0;
    }
    
    // Get physical pointer for direct access
    void* GetHostPointer(u32 virt_addr) const {
        u32 page_idx = virt_addr >> PAGE_SHIFT;
        if (page_idx >= page_table.size()) return nullptr;
        
        const auto& page = page_table[page_idx];
        if (!page.host_ptr || page.type == MemoryType::Unmapped) return nullptr;
        
        u32 page_offset = virt_addr & PAGE_MASK;
        return static_cast<u8*>(page.host_ptr) + page_offset;
    }
    
private:
    std::vector<MemoryPage> page_table;
    
    void* GetPhysicalPointer(u32 phys_addr);  // Defined in mem_map_funcs.cpp
};

// ============================================================================
// Cytrus-compatible memory facade over Mikage's existing Memory namespace API
// ============================================================================

class MemorySystem {
public:
    MemorySystem() : vm_manager(std::make_unique<VirtualMemoryManager>()) {}
    
    // Basic memory access
    u8 Read8(u32 address) const { 
        if (!CheckAccess(address, MemoryPermission::Read)) return 0;
        return Memory::Read8(address); 
    }
    
    u16 Read16(u32 address) const { 
        if (!CheckAccess(address, MemoryPermission::Read)) return 0;
        return Memory::Read16(address); 
    }
    
    u32 Read32(u32 address) const { 
        if (!CheckAccess(address, MemoryPermission::Read)) return 0;
        return Memory::Read32(address); 
    }
    
    u64 Read64(u32 address) const {
        if (!CheckAccess(address, MemoryPermission::Read)) return 0;
        const u64 lo = static_cast<u64>(Read32(address));
        const u64 hi = static_cast<u64>(Read32(address + 4));
        return lo | (hi << 32);
    }
    
    void Write8(u32 address, u8 value) { 
        if (!CheckAccess(address, MemoryPermission::Write)) return;
        Memory::Write8(address, value); 
    }
    
    void Write16(u32 address, u16 value) { 
        if (!CheckAccess(address, MemoryPermission::Write)) return;
        Memory::Write16(address, value); 
    }
    
    void Write32(u32 address, u32 value) { 
        if (!CheckAccess(address, MemoryPermission::Write)) return;
        Memory::Write32(address, value); 
    }
    
    void Write64(u32 address, u64 value) {
        if (!CheckAccess(address, MemoryPermission::Write)) return;
        Write32(address, static_cast<u32>(value & 0xFFFFFFFFULL));
        Write32(address + 4, static_cast<u32>(value >> 32));
    }
    
    // Bulk memory operations
    void ReadBlock(u32 src_addr, void* dest_ptr, u32 size) const {
        if (!CheckAccess(src_addr, MemoryPermission::Read)) return;
        u8* dest = reinterpret_cast<u8*>(dest_ptr);
        for (u32 i = 0; i < size; ++i) {
            dest[i] = this->Read8(src_addr + i);
        }
    }
    
    void WriteBlock(u32 dest_addr, const void* src_ptr, u32 size) {
        if (!CheckAccess(dest_addr, MemoryPermission::Write)) return;
        const u8* src = reinterpret_cast<const u8*>(src_ptr);
        for (u32 i = 0; i < size; ++i) {
            this->Write8(dest_addr + i, src[i]);
        }
    }
    
    // Virtual memory management
    VirtualMemoryManager& GetVMManager() { return *vm_manager; }
    const VirtualMemoryManager& GetVMManager() const { return *vm_manager; }
    
    // Get direct host pointer (for fast access)
    void* GetHostPointer(u32 virt_addr) const {
        return vm_manager->GetHostPointer(virt_addr);
    }
    
    // Check if address is readable/writable/executable
    bool IsReadable(u32 addr) const {
        return vm_manager->IsAccessible(addr, MemoryPermission::Read);
    }
    
    bool IsWritable(u32 addr) const {
        return vm_manager->IsAccessible(addr, MemoryPermission::Write);
    }
    
    bool IsExecutable(u32 addr) const {
        return vm_manager->IsAccessible(addr, MemoryPermission::Execute);
    }
    
private:
    std::unique_ptr<VirtualMemoryManager> vm_manager;
    
    bool CheckAccess(u32 addr, MemoryPermission required_perm) const {
        return vm_manager->IsAccessible(addr, required_perm);
    }
};

// Global memory system instance for CPU access
extern MemorySystem g_memory_system;

// Get the global memory system
inline MemorySystem& GetMemorySystem() {
    return g_memory_system;
}

} // namespace Memory
