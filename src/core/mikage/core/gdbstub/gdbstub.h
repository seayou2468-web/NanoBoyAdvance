#pragma once

#include "../../common/common_types.h"

namespace GDBStub {

enum class BreakpointType {
    Read,
    Write,
    Execute,
};

struct BreakpointAddress {
    u32 address = 0;
    BreakpointType type = BreakpointType::Execute;
};

inline bool IsServerEnabled() {
    return false;
}

inline bool CheckBreakpoint(BreakpointAddress, BreakpointType) {
    return false;
}

inline bool CheckBreakpoint(u32, BreakpointType) {
    return false;
}

inline bool IsMemoryBreak() {
    return false;
}

inline bool GetCpuStepFlag() {
    return false;
}

inline void Break() {}
inline void Break(bool) {}

template <typename ThreadT>
inline void SendTrap(ThreadT*, int) {}

} // namespace GDBStub
