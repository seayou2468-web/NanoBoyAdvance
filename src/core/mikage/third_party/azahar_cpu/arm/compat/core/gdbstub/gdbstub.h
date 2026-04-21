#pragma once

#include <cstdint>

namespace GDBStub {
using BreakpointAddress = uint32_t;
enum class BreakpointType {
    Execute,
    Read,
    Write
};

inline bool IsServerEnabled() {
    return false;
}
inline bool CheckBreakpoint(uint32_t, BreakpointType) {
    return false;
}
inline void Break(bool = false) {}
inline bool IsMemoryBreak() {
    return false;
}
inline bool GetCpuStepFlag() {
    return false;
}
template <typename ThreadT>
inline void SendTrap(ThreadT*, int) {}
}
