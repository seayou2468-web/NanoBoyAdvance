#pragma once

#include "../../helpers.hpp"

namespace GDBStub {
enum class BreakpointType { None, Execute, Read, Write };
struct BreakpointAddress { u32 address = 0; BreakpointType type = BreakpointType::None; };
inline bool IsServerEnabled() { return false; }
inline bool IsMemoryBreak() { return false; }
inline bool IsConnected() { return false; }
inline bool GetCpuStepFlag() { return false; }
inline bool CheckBreakpoint(u32, BreakpointType) { return false; }
inline BreakpointAddress GetNextBreakpointFromAddress(u32, BreakpointType) { return {}; }
inline void Break(bool) {}
}
