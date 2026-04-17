#pragma once
#include "common/common_types.h"
namespace GDBStub {
enum class BreakpointType { None, Read, Write, Execute };
struct BreakpointAddress { u32 address; BreakpointType type; };
inline bool IsServerEnabled() { return false; }
inline bool IsConnected() { return false; }
inline bool CheckBreakpoint(u32, BreakpointType) { return false; }
inline bool IsMemoryBreak() { return false; }
inline void RecordBreak(BreakpointAddress) {}
inline BreakpointAddress GetNextBreakpointFromAddress(u32, BreakpointType) { return {0, BreakpointType::None}; }
}
