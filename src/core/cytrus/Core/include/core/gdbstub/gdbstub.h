#pragma once

#include "common/common_types.h"

namespace Core { class System; }

namespace GDBStub {

enum class BreakpointType : u8 { None = 0, Execute = 1, Read = 2, Write = 3 };

struct BreakpointAddress {
    BreakpointType type{BreakpointType::None};
    VAddr address{};
};

inline bool IsServerEnabled() { return false; }
inline bool IsConnected() { return false; }
inline bool IsMemoryBreak() { return false; }
inline void SetCpuStepFlag(bool) {}
inline void Shutdown() {}
inline void HandlePacket(Core::System&) {}
inline bool GetCpuHaltFlag() { return false; }
inline bool GetCpuStepFlag() { return false; }
inline void SetServerPort(u16) {}
inline void ToggleServer(bool) {}
inline void DeferStart() {}
inline BreakpointAddress GetNextBreakpointFromAddress(VAddr, BreakpointType) { return {}; }

} // namespace GDBStub
