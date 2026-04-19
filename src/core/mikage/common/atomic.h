// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

// iOS-compatible atomic operations using C++11 standard library
#include <atomic>
#include <cstdint>

namespace Common {

// Wrapper for std::atomic to provide common atomic operations
template<typename T>
class AtomicRef {
private:
    std::atomic<T> value;

public:
    AtomicRef(T initial = T()) : value(initial) {}

    inline void Store(T val) {
        value.store(val, std::memory_order_release);
    }

    inline T Load() const {
        return value.load(std::memory_order_acquire);
    }

    inline T Exchange(T val) {
        return value.exchange(val, std::memory_order_acq_rel);
    }

    inline bool CompareExchange(T& expected, T desired) {
        return value.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
    }
};

// Convenience functions for u32 atomics
inline void AtomicAdd(volatile std::atomic<uint32_t>& target, uint32_t value) {
    target.fetch_add(value, std::memory_order_release);
}

inline void AtomicAnd(volatile std::atomic<uint32_t>& target, uint32_t value) {
    target.fetch_and(value, std::memory_order_release);
}

inline void AtomicIncrement(volatile std::atomic<uint32_t>& target) {
    target.fetch_add(1, std::memory_order_release);
}

inline void AtomicDecrement(volatile std::atomic<uint32_t>& target) {
    target.fetch_sub(1, std::memory_order_release);
}

inline uint32_t AtomicLoad(volatile std::atomic<uint32_t>& src) {
    return src.load(std::memory_order_acquire);
}

inline uint32_t AtomicLoadAcquire(volatile std::atomic<uint32_t>& src) {
    return src.load(std::memory_order_acquire);
}

} // namespace Common

#endif
