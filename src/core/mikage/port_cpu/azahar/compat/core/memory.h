// iOS Compatibility Layer for Memory System
// Provides memory access abstraction for CPU interpreter
#pragma once

#include <cstdint>
#include <memory>
#include <array>
#include <cstring>

namespace Memory {

// Page table stub - may be used for MMU operations
class PageTable {
public:
    PageTable() = default;
    ~PageTable() = default;
};

// Memory system interface for CPU interpreter
class MemorySystem {
public:
    MemorySystem() : ram(4 * 1024 * 1024) {} // 4MB default RAM
    ~MemorySystem() = default;
    
    // 8-bit memory access
    uint8_t Read8(uint32_t addr) const {
        if (addr < ram.size()) {
            return ram[addr];
        }
        return 0;
    }
    
    void Write8(uint32_t addr, uint8_t value) {
        if (addr < ram.size()) {
            ram[addr] = value;
        }
    }
    
    // 16-bit memory access
    uint16_t Read16(uint32_t addr) const {
        uint16_t value = 0;
        if (addr + 1 < ram.size()) {
            std::memcpy(&value, &ram[addr], 2);
        }
        return value;
    }
    
    void Write16(uint32_t addr, uint16_t value) {
        if (addr + 1 < ram.size()) {
            std::memcpy(&ram[addr], &value, 2);
        }
    }
    
    // 32-bit memory access
    uint32_t Read32(uint32_t addr) const {
        uint32_t value = 0;
        if (addr + 3 < ram.size()) {
            std::memcpy(&value, &ram[addr], 4);
        }
        return value;
    }
    
    void Write32(uint32_t addr, uint32_t value) {
        if (addr + 3 < ram.size()) {
            std::memcpy(&ram[addr], &value, 4);
        }
    }
    
    // 64-bit memory access
    uint64_t Read64(uint32_t addr) const {
        uint64_t value = 0;
        if (addr + 7 < ram.size()) {
            std::memcpy(&value, &ram[addr], 8);
        }
        return value;
    }
    
    void Write64(uint32_t addr, uint64_t value) {
        if (addr + 7 < ram.size()) {
            std::memcpy(&ram[addr], &value, 8);
        }
    }
    
    // Get page table (stub)
    std::shared_ptr<PageTable> GetPageTable() const {
        return nullptr;
    }
    
    // Invalidate cache range (no-op for compatibility)
    void InvalidateCacheRange(uint32_t addr, std::size_t size) {}
    
private:
    std::vector<uint8_t> ram;
};

} // namespace Memory
