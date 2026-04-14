#pragma once

#include <cstdint>

namespace Core {
class System;
}
namespace Kernel {
class Thread;
}

namespace GDBStub {

enum class BreakpointType { None, Execute, Read, Write };

struct BreakpointAddress {
    std::uint32_t address{};
    BreakpointType type{BreakpointType::None};
};

inline bool IsServerEnabled() { return false; }
inline bool IsConnected() { return false; }
inline bool IsMemoryBreak() { return false; }
inline bool GetCpuHaltFlag() { return false; }
inline bool GetCpuStepFlag() { return false; }
inline bool CheckBreakpoint(std::uint32_t, BreakpointType) { return false; }
inline void SetCpuStepFlag(bool) {}
inline void Break(bool = false) {}
inline void SendTrap(Kernel::Thread*, int) {}
inline void SetHioRequest(Core::System&, std::uint32_t) {}
inline BreakpointAddress GetNextBreakpointFromAddress(std::uint32_t, BreakpointType) {
    return {};
}
inline void HandlePacket(Core::System&) {}
inline void DeferStart() {}
inline void Shutdown() {}
inline void ToggleServer(bool) {}
inline void SetServerPort(std::uint16_t) {}

} // namespace GDBStub
