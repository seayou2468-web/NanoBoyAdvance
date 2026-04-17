// iOS Compatibility Layer for GDB Debug Stub
// Provides stubs for debugging operations
#pragma once

#include <cstdint>

namespace GDBStub {

// Debug break types
enum BreakpointType {
    BKPT_SOFT,
    BKPT_HARD
};

// All GDB stub functions are stubs for iOS (debugging via Xcode)
inline bool IsServerEnabled() {
    return false; // GDB server disabled on iOS
}

inline bool IsMemoryBreak() {
    return false;
}

inline bool IsConnected() {
    return false; // No remote GDB on iOS
}

inline BreakpointType GetNextBreakpointFromAddress(uint32_t addr, bool is_memory) {
    return BKPT_SOFT;
}

inline void ToggleBreakpoint(uint32_t addr, bool memory_breakpoint) {
    // No-op on iOS
}

inline void CheckBreakpoints(uint32_t addr) {
    // No-op on iOS
}

inline void Break() {
    // No-op on iOS - use Xcode debugger instead
}

} // namespace GDBStub
