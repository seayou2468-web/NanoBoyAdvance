#pragma once

#include <cstddef>

#include "common/common_types.h"

class BackingMem {
public:
    virtual ~BackingMem() = default;
    virtual u8* GetPtr() = 0;
    virtual const u8* GetPtr() const = 0;
    virtual std::size_t GetSize() const = 0;
};

class MemoryRef {
public:
    constexpr MemoryRef() = default;
    constexpr MemoryRef(std::nullptr_t) : ptr(nullptr) {}
    explicit constexpr MemoryRef(u8* p) : ptr(p) {}

    constexpr u8* GetPtr() const {
        return ptr;
    }

    constexpr MemoryRef operator+(std::size_t offset) const {
        return MemoryRef(ptr ? ptr + offset : nullptr);
    }

    constexpr MemoryRef& operator=(std::nullptr_t) {
        ptr = nullptr;
        return *this;
    }

private:
    u8* ptr = nullptr;
};
