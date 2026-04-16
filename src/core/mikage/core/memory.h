#pragma once

#include "./mem_map.h"

namespace Memory {

// Cytrus-compatible memory facade over Mikage's existing Memory namespace API.
class MemorySystem {
public:
    u8 Read8(u32 address) const { return Memory::Read8(address); }
    u16 Read16(u32 address) const { return Memory::Read16(address); }
    u32 Read32(u32 address) const { return Memory::Read32(address); }
    u64 Read64(u32 address) const {
        const u64 lo = static_cast<u64>(Read32(address));
        const u64 hi = static_cast<u64>(Read32(address + 4));
        return lo | (hi << 32);
    }

    void Write8(u32 address, u8 value) { Memory::Write8(address, value); }
    void Write16(u32 address, u16 value) { Memory::Write16(address, value); }
    void Write32(u32 address, u32 value) { Memory::Write32(address, value); }
    void Write64(u32 address, u64 value) {
        Write32(address, static_cast<u32>(value & 0xFFFFFFFFULL));
        Write32(address + 4, static_cast<u32>(value >> 32));
    }
};

// Global memory system instance for CPU access
extern MemorySystem g_memory_system;

// Get the global memory system
inline MemorySystem& GetMemorySystem() {
    return g_memory_system;
}

} // namespace Memory
