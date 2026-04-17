// iOS Compatibility Layer for SVC/SWI Handler
// Supervisor Call and Software Interrupt stubs
#pragma once

#include <cstdint>

namespace HLE::Kernel {

// SVC numbers for ARM SVC calls
enum SVCNumber {
    SVC_GetResult = 0x00,
    SVC_ExitThread = 0x01,
    SVC_SleepThread = 0x02,
    SVC_GetThreadPriority = 0x03,
    // ... more SVCs can be added as needed
};

// Handle SVC calls - stub implementation
inline void HandleSVC(uint32_t svc_number) {
    // No-op for iOS - most SVCs are system/kernel calls
    // which are not relevant for CPU emulation
}

} // namespace HLE::Kernel
