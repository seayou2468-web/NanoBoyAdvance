// Copyright 2026 Aurora Project
// Licensed under GPLv2 or any later version

#include <cstring>
#include "kernel/kernel.hpp"
#include "memory.hpp"

void* Memory::getReadPointer(u32 address) {
    const auto& vm = activeVM();
    const std::size_t page = address >> pageShift;
    if (page >= vm.readTable.size()) {
        return nullptr;
    }
    const uintptr_t base = vm.readTable[page];
    if (base == 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(base + (address & pageMask));
}

void* Memory::getWritePointer(u32 address) {
    const auto& vm = activeVM();
    const std::size_t page = address >> pageShift;
    if (page >= vm.writeTable.size()) {
        return nullptr;
    }
    const uintptr_t base = vm.writeTable[page];
    if (base == 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(base + (address & pageMask));
}

u8 Memory::read8(u32 vaddr) {
    if (auto* p = static_cast<u8*>(getReadPointer(vaddr)); p != nullptr) {
        return *p;
    }
    return 0;
}

u16 Memory::read16(u32 vaddr) {
    u16 out = 0;
    if (auto* p = getReadPointer(vaddr); p != nullptr) {
        std::memcpy(&out, p, sizeof(out));
    }
    return out;
}

u32 Memory::read32(u32 vaddr) {
    u32 out = 0;
    if (auto* p = getReadPointer(vaddr); p != nullptr) {
        std::memcpy(&out, p, sizeof(out));
    }
    return out;
}

u64 Memory::read64(u32 vaddr) {
    u64 out = 0;
    if (auto* p = getReadPointer(vaddr); p != nullptr) {
        std::memcpy(&out, p, sizeof(out));
    }
    return out;
}

void Memory::write8(u32 vaddr, u8 value) {
    if (auto* p = static_cast<u8*>(getWritePointer(vaddr)); p != nullptr) {
        *p = value;
    }
}

void Memory::write16(u32 vaddr, u16 value) {
    if (auto* p = getWritePointer(vaddr); p != nullptr) {
        std::memcpy(p, &value, sizeof(value));
    }
}

void Memory::write32(u32 vaddr, u32 value) {
    if (auto* p = getWritePointer(vaddr); p != nullptr) {
        std::memcpy(p, &value, sizeof(value));
    }
}

void Memory::write64(u32 vaddr, u64 value) {
    if (auto* p = getWritePointer(vaddr); p != nullptr) {
        std::memcpy(p, &value, sizeof(value));
    }
}

void Kernel::serviceSVC(u32 svc) {
    // Direct CPU<->Kernel connection point for integration bring-up.
    // Full syscall dispatch is still handled in the legacy HLE service implementation.
    (void)svc;
}
