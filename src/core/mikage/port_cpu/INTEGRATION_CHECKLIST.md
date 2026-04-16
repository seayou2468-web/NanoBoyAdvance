# 3DS CPU Port - Integration Checklist & Dependency Map

## Critical Path Dependencies

```
PHASE 2: SHIM LAYER (Days 1-7)
└─ GDBStub.h stub
   └─ Memory::MemorySystem verification
      └─ Core::System compatibility check
         └─ core_timing::Timer integration
            └─ common/microprofile.h stub
               └─ core/hle/kernel/svc.h stub

PHASE 3: COMPATIBILITY SHIM (Days 8-17)
└─ ARMul_State constructor wiring
   ├─ CP15 register translation layer
   ├─ VFP register translation layer
   ├─ Memory read/write hook testing
   └─ Instruction decode & execute testing

PHASE 4: DUAL-PATH (Days 18-22)
└─ Compile-time CPU selection switch
   └─ Legacy vs DynCom trace comparison

PHASE 5: CUTOVER (Days 23-25)
└─ Legacy interpreter removal
```

---

## Dependency Verification Checklist

### MUST EXIST (or create stub)

- [ ] `core/gdbstub/gdbstub.h` - **MISSING** → **CREATE STUB**
- [ ] `Memory::MemorySystem` in `core/memory.h` - **VERIFY EXISTS**
- [ ] `Core::System` class - **VERIFY EXISTS**
- [ ] `Core::Timing::Timer` - **VERIFY EXISTS**
- [ ] `common/microprofile.h` - **MISSING** → **CREATE STUB**
- [ ] `core/hle/kernel/svc.h` - **VERIFY EXISTS OR CREATE STUB**

### OPTIONAL (can stub)

- [ ] `boost/serialization/shared_ptr.hpp` - **OPTIONAL** → Can remove or keep

---

## File-by-File Integration Points

### CRITICAL: arm_dyncom_interpreter.cpp (4,590 LOC)

**Includes Required**:
```cpp
#include "common/common_types.h"           // ✅ Likely exists
#include "common/logging/log.h"            // ✅ Likely exists
#include "common/microprofile.h"           // ❌ CREATE STUB
#include "core/arm/dyncom/arm_dyncom_*.h"  // ✅ In port_cpu
#include "core/arm/skyeye_common/*"        // ✅ In port_cpu
#include "core/core.h"                     // ✅ Verify exists
#include "core/core_timing.h"              // ✅ Verify exists
#include "core/gdbstub/gdbstub.h"          // ❌ CREATE STUB
#include "core/hle/kernel/svc.h"           // ❌ CREATE STUB (or check)
#include "core/memory.h"                   // ✅ Verify Memory::MemorySystem
```

**GDB Integration Points**:
```cpp
// Line 922-960: GDBStub::BreakpointAddress breakpoint_data;
if (GDBStub::IsServerEnabled()) {
    if (GDBStub::IsMemoryBreak()) { ... }
    else if (breakpoint_data.type != GDBStub::BreakpointType::None && ...) { ... }
}

// Line 1656-1658: GDBStub::IsConnected() check
if (GDBStub::IsConnected()) {
    breakpoint_data = GDBStub::GetNextBreakpointFromAddress(cpu->Reg[15], ...);
}
```

**Stub Replacement (if disabling GDB)**:
```cpp
namespace GDBStub {
    struct BreakpointAddress { u32 address = 0; };
    enum class BreakpointType { None = 0 };
    inline bool IsServerEnabled() { return false; }
    inline bool IsMemoryBreak() { return false; }
    inline bool IsConnected() { return false; }
    inline BreakpointAddress GetNextBreakpointFromAddress(u32, BreakpointType) { return {}; }
    // ... other no-ops
}
```

---

### CRITICAL: armstate.cpp (620 LOC)

**Includes Required**:
```cpp
#include "common/common_types.h"                    // ✅
#include "core/arm/skyeye_common/arm_regformat.h"  // ✅
#include "core/gdbstub/gdbstub.h"                  // ❌ STUB
#include "core/memory.h"                           // ✅ VERIFY
```

**Constructor**:
```cpp
ARMul_State::ARMul_State(Core::System& system,
                         Memory::MemorySystem& memory,
                         PrivilegeMode initial_mode)
    : system(system), memory(memory), ...
{
    ChangePrivilegeMode(initial_mode);
    Reset();
}
```

**INTEGRATION REQUIRED**:
- Verify `Core::System&` reference is available at ARM_DynCom instantiation
- Verify `Memory::MemorySystem&` has read/write methods matching calls below

**Memory Access Hooks** (called from instruction executor):
```cpp
// Line 197+: CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);
u8 ARMul_State::ReadMemory8(u32 address) const {
    CheckMemoryBreakpoint(address, GDBStub::BreakpointType::Read);
    return InBigEndianMode() 
        ? ... 
        : memory.Read8(address);  // ← CALL SITE: Must match Memory::MemorySystem API
}
```

**CP15 Coprocessor** (line 267+):
```cpp
u32 ARMul_State::ReadCP15Register(u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) const {
    // Switch on CRn; return CP15[index]
    // Some registers trigger side effects (cache control, TLB flush, etc)
}
```

**Test After Integration**:
1. Instantiate ARMul_State with mock System and MemorySystem
2. Verify constructor doesn't crash
3. Call ReadMemory32/WriteMemory32 with test addresses
4. Verify memory callbacks routed correctly

---

### CRITICAL: arm_dyncom.cpp (128 LOC)

**Constructor**:
```cpp
ARM_DynCom::ARM_DynCom(Core::System& system,
                       Memory::MemorySystem& memory,
                       PrivilegeMode initial_mode,
                       u32 id,
                       std::shared_ptr<Core::Timing::Timer> timer)
    : ARM_Interface(id, timer),
      system(system),
      state(std::make_unique<ARMul_State>(system, memory, initial_mode))
{
}
```

**INTEGRATION REQUIRED**:
- [ ] Verify Core::System API compatibility
- [ ] Verify Memory::MemorySystem API compatibility
- [ ] Verify Core::Timing::Timer exists
- [ ] Update core.cpp CPU factory to call this constructor

**Key Methods**:
```cpp
void ARM_DynCom::Run() {
    ExecuteInstructions(1);  // Execute 1 at a time or batch?
}

void ARM_DynCom::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    // Update TLB? Or just store for later?
}
```

---

## Dependency Interface Specifications

### Memory::MemorySystem Required Interface

```cpp
class MemorySystem {
public:
    u8 Read8(u32 address);
    u16 Read16(u32 address);
    u32 Read32(u32 address);
    u64 Read64(u32 address);
    
    void Write8(u32 address, u8 value);
    void Write16(u32 address, u16 value);
    void Write32(u32 address, u32 value);
    void Write64(u32 address, u64 value);
    
    // Optional: endianness support
    bool IsBigEndian() const;  // Or check CPSR in CPU
};
```

**VERIFICATION STEPS**:
1. Locate `core/memory.h` in Mikage
2. Confirm class exists with above methods
3. If names differ, create adapter:
```cpp
// adapter.h
class MemorySystemAdapter : public Memory::MemorySystem {
    u8 Read8(u32 addr) override { return legacy_mem_read_byte(addr); }
    // ... etc
};
```

---

### Core::System Required Interface

```cpp
class System {
public:
    // Called during ARMul_State construction
    // Should provide timing, event queue, or other integration
    // Specific methods TBD by Mikage code inspection
};
```

**VERIFICATION STEPS**:
1. Locate `core/core.h` in Mikage
2. Check what methods System provides
3. Check if CPU instances already receive System reference
4. If not, add parameter to CPU constructor

---

### Core::Timing::Timer Interface

```cpp
class Timer {
public:
    // Typically used for:
    // - Queuing events
    // - Tracking elapsed cycles
    u64 GetCycles() const;
    void ScheduleEvent(...);
};
```

**VERIFICATION**: Usually non-blocking if already used by emulator.

---

## GDBStub Stub: Complete Reference Implementation

```cpp
// file: core/gdbstub/gdbstub.h
#pragma once

#include "common/common_types.h"

namespace GDBStub {

// Breakpoint type enumeration
enum class BreakpointType : u8 {
    None = 0,
    Execute = 1,
    Read = 2,
    Write = 3,
};

// Breakpoint address record
struct BreakpointAddress {
    u32 address = 0;
    BreakpointType type = BreakpointType::None;
};

// Server state (all returns false = disabled)
inline bool IsServerEnabled() { return false; }
inline bool IsConnected() { return false; }
inline bool IsMemoryBreak() { return false; }
inline bool GetCpuStepFlag() { return false; }

// Breakpoint operations (all no-ops)
inline bool CheckBreakpoint(u32 address, BreakpointType type) { return false; }
inline BreakpointAddress GetNextBreakpointFromAddress(u32 addr, BreakpointType type) { 
    return BreakpointAddress{addr, BreakpointType::None}; 
}

inline void Break() {}
inline void Break(bool value) {}
inline void SendTrap(void* thread, int signal) {}

} // namespace GDBStub
```

---

## Integration Testing Checklist

### Unit Tests (Phase 2)

```cpp
✅ TEST: ARMul_State construction
   ARMul_State state(system, memory, USER32MODE);
   ASSERT_EQ(state.Mode, USER32MODE);

✅ TEST: Memory read/write round-trip
   state.WriteMemory32(0x1000, 0xDEADBEEF);
   ASSERT_EQ(state.ReadMemory32(0x1000), 0xDEADBEEF);

✅ TEST: Register access
   state.Reg[0] = 42;
   ASSERT_EQ(state.Reg[0], 42);

✅ TEST: Privilege mode switch
   state.ChangePrivilegeMode(SVC32MODE);
   ASSERT_EQ(state.Mode, SVC32MODE);

✅ TEST: CP15 register read/write
   state.WriteCP15Register(0xDEADBEEF, 0, 0, 1, 0);
   ASSERT_EQ(state.ReadCP15Register(0, 0, 1, 0), 0xDEADBEEF);
```

### Integration Tests (Phase 3)

```cpp
✅ TEST: Execute single ARM instruction (MOV R0, #1)
   ARM_DynCom cpu(system, memory, USER32MODE, 0, timer);
   cpu.SetReg(15, 0x1000);  // PC
   memory.Write32(0x1000, 0xE3A00001);  // MOV R0, #1 (ARM encoding)
   cpu.Step();
   ASSERT_EQ(cpu.GetReg(0), 1);  // R0 = 1

✅ TEST: Execute Thumb instruction
   cpu.SetCPSR(cpu.GetCPSR() | 0x20);  // Set T bit (Thumb mode)
   memory.Write16(0x1000, 0x2001);  // MOVS R0, #1 (Thumb encoding)
   cpu.Step();
   ASSERT_EQ(cpu.GetReg(0), 1);

✅ TEST: Branch
   memory.Write32(0x1000, 0xEA000001);  // B 0x1008 (offset +2 instructions)
   cpu.Step();
   ASSERT_EQ(cpu.GetPC(), 0x1008);

✅ TEST: VFP add (FADDS S0, S1, S2)
   cpu.SetVFPReg(1, float_bits(1.0f));
   cpu.SetVFPReg(2, float_bits(2.0f));
   memory.Write32(0x1000, 0xEE300A81);  // FADDS S0, S1, S2
   cpu.Step();
   ASSERT_EQ(bits_to_float(cpu.GetVFPReg(0)), 3.0f);
```

---

## Pre-Integration Verification Checklist

### Before Phase 2

- [ ] Locate `src/core/mikage/core/memory.h` and document Memory::MemorySystem interface
- [ ] Locate `src/core/mikage/core/core.h` and document Core::System interface
- [ ] Locate `src/core/mikage/core/core_timing.h` and document Core::Timing::Timer interface
- [ ] **If not found**, add note in phase 2 blocker list
- [ ] Check if `core/hle/kernel/svc.h` exists; if not, note for stub creation
- [ ] Check if `common/microprofile.h` exists; if not, note for stub creation
- [ ] Create `core/gdbstub/gdbstub.h` stub (reference implementation above)

### Before Phase 3

- [ ] Verify all includes in arm_dyncom_interpreter.cpp resolve
- [ ] Verify all includes in armstate.cpp resolve
- [ ] Verify all includes in arm_dyncom.cpp resolve
- [ ] Create adapter layer (if needed for Memory::MemorySystem)
- [ ] Create unit test framework (Google Test or simple assert macros)
- [ ] Create mock System and MemorySystem classes for unit testing

### Before Phase 4

- [ ] All Phase 2 & 3 tests passing
- [ ] Legacy interpreter still compiling and passing old tests
- [ ] Compile switch (`USE_DYNCOM_CPU`) implemented and tested

---

## Known Issues & Potential Gotchas

### 1. **Endianness Handling**
**Location**: `armstate.cpp` lines 197-226 (ReadMemory* functions)  
**Issue**: CPU checks E-bit in CPSR to determine endianness  
**Action**:
```cpp
u32 ARMul_State::ReadMemory32(u32 address) const {
    if (InBigEndianMode()) {
        // Byte reverse: 0x12345678 → 0x78563412
        return ((memory.Read8(address) << 24) |
                (memory.Read8(address+1) << 16) |
                (memory.Read8(address+2) << 8) |
                (memory.Read8(address+3) << 0));
    } else {
        return memory.Read32(address);  // Little-endian (common case)
    }
}
```

### 2. **Memory Access with GDB Breakpoints**
**Location**: `armstate.cpp` lines 188-191  
**Issue**: Every memory access can trigger breakpoint  
**Action**: Stub GDBStub::CheckBreakpoint() to return false in iOS build

### 3. **CP15 Cache Control Side Effects**
**Location**: `armstate.cpp` lines 267+  
**Issue**: Writing CP15 registers may flush caches or invalidate TLBs  
**Action**: For MVP, just store value; later add MMU/cache hooks

### 4. **VFP Denormalized Numbers**
**Location**: `vfpsingle.cpp`, `vfpdouble.cpp`  
**Issue**: IEEE 754 denormalized handling is subtle; rounding modes matter  
**Action**: Use test ROMs that exercise VFP edge cases

### 5. **Exclusive Monitor Atomicity**
**Location**: `exclusive_monitor.cpp` line 27  
**Issue**: LDREX/STREX in multi-core need synchronization  
**Action**: For single-core, simple flag works; multi-core needs mutex

---

## Success Criteria

### Phase 2 Complete When
- [ ] All 4 dependency stubs created and compiling
- [ ] All includes in port_cpu resolve without errors
- [ ] ARMul_State constructor callable with real System & MemorySystem
- [ ] ARMul_State memory access methods callable

### Phase 3 Complete When
- [ ] All unit tests pass (arith, memory, register, CP15)
- [ ] Single instruction execution works for at least 10 ARM and 10 Thumb opcodes
- [ ] VFP single-precision ops work
- [ ] Branch instructions update PC correctly

### Phase 4 Complete When
- [ ] Legacy and DynCom traces match for 100+ instruction sequences
- [ ] Register state identical after each step
- [ ] Memory state identical
- [ ] Flags (N/Z/C/V) identical
- [ ] Zero divergence detected in comparison

### Phase 5 Complete When
- [ ] Legacy interpreter removed
- [ ] All integration tests pass
- [ ] ROM corpus (5-20 games) boots and runs
- [ ] Performance metrics recorded

---

## Performance Baseline (Reference)

**For later optimization comparison**:
- [ ] Measure: Instructions/second with DynCom
- [ ] Measure: VFP ops/second
- [ ] Measure: Memory bandwidth (R/W operations)
- [ ] Compare: Legacy interpreter throughput (may be slower on ARM)

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-16  
**Next Review**: After Phase 2 completion
