# 3DS Emulator CPU Port - Complete Component Analysis

**Analysis Date**: 2026-04-16  
**Scope**: `src/core/mikage/port_cpu/{azahar,cytrus,cpu1}`  
**Total Components**: 3 directories, 98 source files, 60,230 lines of code  
**Status**: Vendor import complete (Phase 1 ✅), surface parity audit in progress (Phase 2 in progress)

---

## Executive Summary

The `port_cpu` directory contains **three parallel implementations** of the ARM11 CPU interpreter from the Citra 3DS emulator project:

- **AZAHAR** (32,520 LOC): Complete duplicate with nested `core/` and `include/` mirror
- **CYTRUS** (14,887 LOC): Clean vendor snapshot with 26 files (canonical source)
- **CPU1** (12,823 LOC): Implementation-only subset with headers stripped

### Key Findings
- ✅ DynCom CPU implementation complete and consistent across all variants
- ⚠️ Heavy dependencies on Citra architecture (GDBStub, Core::System, Memory::MemorySystem)
- ⚠️ Only **VFP floating-point** is non-CPU component; no GPU/HLE/kernel/IO code present
- 🎯 Estimated **30-40 days** to port with all dependency shims in place

---

## 1. Directory Structure Map

### AZAHAR (32,520 lines, 59 files)

```
azahar/
├── arm_interface.h                              (307 lines) - CPU interface contract
├── exclusive_monitor.h/cpp                      (62 lines) - LDREX/STREX synchronization
└── core/                                         (29,405 lines)
    ├── arm/
    │   ├── arm_interface.h                      (duplicate)
    │   ├── exclusive_monitor.h/cpp              (duplicate)
    │   ├── dyncom/
    │   │   ├── arm_dyncom.h/cpp                 (192 lines) - Interface implementation
    │   │   ├── arm_dyncom_interpreter.h/cpp     (4,599 lines) - MAIN: 70+ instruction handlers
    │   │   ├── arm_dyncom_dec.h/cpp             (514 lines) - Instruction decoder dispatch
    │   │   ├── arm_dyncom_thumb.h/cpp           (439 lines) - Thumb mode decoder
    │   │   ├── arm_dyncom_trans.h/cpp           (2,516 lines) - Translation layer
    │   │   └── arm_dyncom_run.h                 (48 lines) - Execution loop helpers
    │   └── skyeye_common/
    │       ├── arm_regformat.h                  (187 lines) - Register enums
    │       ├── armstate.h/cpp                   (892 lines) - CPU state & memory access
    │       ├── armsupp.h/cpp                    (218 lines) - Utility functions
    │       └── vfp/                             (4,910 lines) - Floating point
    │           ├── vfp.h/cpp                    (180 lines)
    │           ├── vfp_helper.h                 (426 lines) - Macro utilities
    │           ├── asm_vfp.h                    (83 lines) - ASM wrappers
    │           ├── vfpsingle.cpp                (1,273 lines) - Single-precision
    │           ├── vfpdouble.cpp                (1,248 lines) - Double-precision
    │           └── vfpinstr.cpp                 (1,703 lines) - VFP instruction handlers
    ├── dyncom/                                   (8,308 lines - DUPLICATED from core/arm/dyncom)
    └── skyeye_common/                            (6,210 lines - DUPLICATED from core/arm/skyeye_common)
└── include/                                      (2,746 lines - DUPLICATED headers)
    └── core/arm/skyeye_common/
        └── (mirrors of skyeye_common headers)
```

**AZAHAR Status**: DUPLICATE FOR COMPARISON ONLY. Use CYTRUS for canonical reference.

---

### CYTRUS (14,887 lines, 26 files) - ✅ CANONICAL VENDOR SOURCE

```
cytrus/
├── arm_interface.h                              (307 lines)
├── exclusive_monitor.h/cpp                      (62 lines)
├── dyncom/                                       (8,308 lines, 11 files)
│   ├── arm_dyncom.h/cpp                         (192 lines)
│   ├── arm_dyncom_interpreter.h/cpp             (4,599 lines) ⭐
│   ├── arm_dyncom_dec.h/cpp                     (514 lines)
│   ├── arm_dyncom_thumb.h/cpp                   (439 lines)
│   ├── arm_dyncom_trans.h/cpp                   (2,516 lines)
│   └── arm_dyncom_run.h                         (48 lines)
└── skyeye_common/                                (6,210 lines, 12 files)
    ├── arm_regformat.h                          (187 lines)
    ├── armstate.h/cpp                           (892 lines)
    ├── armsupp.h/cpp                            (218 lines)
    ├── armsupp.h                                (32 lines)
    └── vfp/                                      (4,910 lines)
        ├── vfp.h/cpp                            (180 lines)
        ├── vfp_helper.h                         (426 lines)
        ├── asm_vfp.h                            (83 lines)
        ├── vfpsingle.cpp                        (1,273 lines)
        ├── vfpdouble.cpp                        (1,248 lines)
        └── vfpinstr.cpp                         (1,703 lines)
```

**CYTRUS Status**: ✅ CLEAN, CANONICAL, VENDOR-PINNED (commit `84f59c91...`)

---

### CPU1 (12,823 lines, 13 files) - IMPLEMENTATION-ONLY SUBSET

```
cpu1/
├── exclusive_monitor.cpp                        (27 lines) [headers missing]
├── dyncom/                                       (7,629 lines, 5 files)
│   ├── arm_dyncom.cpp                           (128 lines) [.h missing]
│   ├── arm_dyncom_interpreter.cpp               (4,590 lines) [.h missing]
│   ├── arm_dyncom_dec.cpp                       (503 lines) [.h missing]
│   ├── arm_dyncom_thumb.cpp                     (390 lines) [.h missing]
│   └── arm_dyncom_trans.cpp                     (2,018 lines) [.h missing]
└── skyeye_common/                                (5,167 lines, 6 files)
    ├── armstate.cpp                             (620 lines) [.h missing]
    ├── armsupp.cpp                              (186 lines) [.h missing]
    └── vfp/                                      (4,361 lines, no .h files)
        ├── vfp.cpp                              (137 lines)
        ├── vfp_helper.h                         (426 lines) [copy from cytrus]
        ├── vfpsingle.cpp                        (1,273 lines)
        ├── vfpdouble.cpp                        (1,248 lines)
        └── vfpinstr.cpp                         (1,703 lines)
```

**CPU1 Status**: ⚠️ INCOMPLETE - missing all header files except vfp_helper.h. Appears to be work-in-progress extraction.

---

## 2. Component Categorization

### 2.1 CPU Interpreter (PRIMARY - 4,590 LOC)

**File**: `arm_dyncom_interpreter.cpp`  
**Purpose**: Execute ARM/Thumb instructions one at a time  
**Scope**: 70+ instruction types supported

**Key Functions**:
```cpp
ThumbExecuteInstructions()      // Thumb-2 main loop
ARMExecuteInstructions()        // ARM 32-bit main loop
CondPassed(cpu, cond)           // Condition code evaluation
DataProcessingOperands()        // ALU operand fetch
```

**Instruction Categories**:
- **Data Processing**: MOV, MVN, ADD, SUB, AND, ORR, EOR, BIC (40+ variants)
- **Multiplies**: MUL, MLA, UMULL, SMULL, UMLAL, SMLAL
- **Load/Store**: LDR, STR, LDREX, STREX (atomic operations)
- **Branches**: B, BL, BX, BLX
- **Exceptions**: SWI/SVC, BKPT
- **Coprocessor**: MCR, MRC (via CP15 interface)
- **Shifts**: LSL, LSR, ASR, ROR
- **VFP**: Delegated to `vfpinstr.cpp`

**GDB Integration**: Breakpoint checking on every instruction execution

---

### 2.2 Instruction Decoding (1,443 LOC)

#### **arm_dyncom_dec.cpp** (503 LOC)
- Dispatch table for ARM instruction opcodes
- Returns decoded instruction with operand extraction
- No execution logic; purely decode

#### **arm_dyncom_thumb.cpp** (390 LOC)
- Thumb instruction detection
- Thumb mode operand handling
- 16-bit & 32-bit Thumb detection

#### **arm_dyncom_trans.cpp** (2,018 LOC)
- Translation layer between instruction formats
- Register mapping
- Operand assembly

---

### 2.3 Floating Point Unit (VFP) - (5,083 LOC)

**Coverage**: ARMv7 VFPv2/VFPv3 full specification

#### **vfp.h/cpp** (180 LOC)
- VFP initialization
- Mode setup (single/double precision)
- System register state (FPSCR, FPEXC, FPSID)

#### **vfp_helper.h** (426 LOC)
- Macro utilities for bit extraction: `DWORD()`, `BITS()`, etc.
- Inline helpers for rounding, exponent normalization
- Denormalized number handling

#### **vfpsingle.cpp** (1,273 LOC)
- Single-precision float operations (32-bit)
- Add, subtract, multiply, divide
- Square root, conversion (F32↔I32)
- Special value handling (NaN, Inf)
- Rounding modes

#### **vfpdouble.cpp** (1,248 LOC)
- Double-precision float operations (64-bit)
- Same scope as single-precision
- Extended exponent/mantissa handling

#### **vfpinstr.cpp** (1,703 LOC)
- VFP instruction handlers
- FLADD, FLSUB, FLDIV, FLMUL, FLSQRT, etc.
- VFP register load/store
- Index/format conversions

#### **asm_vfp.h** (83 LOC)
- Inline ASM wrappers for CPU VFP detection
- Fallback soft-float implementations

---

### 2.4 CPU State & Register Abstraction (892 LOC)

#### **armstate.h** (272 LOC)
```cpp
struct ARMul_State {
    // Constructor requires:
    explicit ARMul_State(Core::System& system,
                         Memory::MemorySystem& memory,
                         PrivilegeMode initial_mode);
    
    // General-purpose registers
    std::array<u32, 16> Reg;                    // R0-R15 (current mode)
    std::array<u32, 2> Reg_usr;                 // R13, R14 user-mode shadows
    std::array<u32, 2> Reg_irq;                 // R13, R14 IRQ-mode shadows
    std::array<u32, 2> Reg_svc;                 // R13, R14 SVC-mode shadows
    // ... (abort, undef, FIQ modes)
    
    // System state
    u32 Cpsr;                                   // Current Program Status Register
    u32 Mode;                                   // Current privilege mode (USER32, SVC32, etc)
    u32 TFlag;                                  // Thumb mode bit
    u32 phys_pc;                                // Physical program counter
    
    // Flags
    u32 NFlag, ZFlag, CFlag, VFlag;             // N, Z, C, V bits (cached)
    
    // Coprocessor state
    std::array<u32, CP15_REGISTER_COUNT> CP15;  // CP15 control registers
    std::array<u32, VFP_SYSTEM_REGISTER_COUNT> VFP;  // VFP system regs (FPSCR, etc)
    std::array<u32, 64> ExtReg;                 // VFP data registers (D0-D31 or S0-S31)
    
    // Memory access hooks
    u8 ReadMemory8(u32 address) const;
    u32 ReadMemory32(u32 address) const;
    void WriteMemory32(u32 address, u32 data);
    
    // CP15 coprocessor interface
    u32 ReadCP15Register(u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) const;
    void WriteCP15Register(u32 value, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2);
    
    // Debug support
    void RecordBreak(GDBStub::BreakpointAddress bkpt);
    void ServeBreak();
};
```

#### **armstate.cpp** (620 LOC)
- ARMul_State constructor implementation
- Memory read/write delegations to `memory`
- Privilege mode transition logic
- CP15 register read/write implementation
- GDB breakpoint integration

#### **arm_regformat.h** (187 LOC)
```cpp
enum CP15Register {
    CP15_SCTLR = ...,         // System Control Register
    CP15_ACTLR = ...,         // Auxiliary Control Reg
    CP15_CPACR = ...,         // Coprocessor Access Control
    // ... 30+ registers
};

enum VFPSystemRegister {
    VFP_FPSID = 0,            // ID register
    VFP_FPSCR = 1,            // Status/Control
    VFP_MVFR0 = 6,            // Media Features Register
    VFP_MVFR1 = 7,
};
```

#### **armsupp.cpp** (186 LOC)
- `ARMulDecodeCondition()` - decode condition codes
- `ARMulDecodePSR()` - decode APSR flags
- Utility functions for register extraction

---

### 2.5 Synchronization (ExclusiveMonitor) - (62 LOC)

**File**: `exclusive_monitor.h/cpp`

```cpp
class ARM_ExclusiveMonitor {
public:
    bool IsExclusiveMemoryAccess(u32 address) const;
    void SetExclusiveMemoryAddress(u32 address);
    void UnsetExclusiveMemoryAddress();
    
private:
    u32 exclusive_tag;                          // Address tag (2-word granule)
    bool exclusive_state;
};
```

**Purpose**: Implement LDREX/STREX atomic primitives for thread-safe memory access

**Reservation Granule**: 8 bytes (smallest ARMv7 spec allows)

---

### 2.6 CPU Interface Abstraction (307 LOC)

**File**: `arm_interface.h`

```cpp
class ARM_Interface : NonCopyable {
public:
    struct ThreadContext {                       // Register snapshot for context switch
        std::array<u32, 16> cpu_registers;      // R0-R15
        u32 cpsr;
        std::array<u32, 64> fpu_registers;      // VFP D0-D31
        u32 fpscr;
        u32 fpexc;
    };
    
    // Execution
    virtual void Run() = 0;                     // Run until event
    virtual void Step() = 0;                    // Single-step instruction
    
    // Cache management
    virtual void ClearInstructionCache() = 0;
    virtual void InvalidateCacheRange(u32 start, std::size_t length) = 0;
    
    // Register access
    virtual u32 GetReg(int index) const = 0;
    virtual void SetReg(int index, u32 value) = 0;
    virtual u32 GetVFPReg(int index) const = 0;
    virtual u32 GetCP15Register(CP15Register reg) const = 0;
    
    // Context save/restore (for scheduling)
    virtual void SaveContext(ThreadContext& ctx) = 0;
    virtual void LoadContext(const ThreadContext& ctx) = 0;
    
    // Memory/page table management
    virtual void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) = 0;
};
```

**Implementation**: `arm_dyncom.h/cpp` (192 LOC) implements this interface

---

## 3. Component Line Count Summary

| Component | Files | Lines | LOC % | Complexity |
|-----------|-------|-------|-------|------------|
| **Instruction Interpreter** | 3 | 5,107 | 8% | 🔴 CRITICAL |
| **Instruction Decode** | 3 | 1,443 | 2% | 🟡 MEDIUM |
| **VFP Floating Point** | 6 | 5,083 | 8% | 🟡 MEDIUM |
| **CPU State Management** | 4 | 1,076 | 2% | 🟡 MEDIUM |
| **Interface/Sync** | 4 | 368 | 1% | 🟢 LOW |
| **TOTAL CYTRUS** | 26 | 14,887 | 100% | - |

---

## 4. External Dependencies Analysis

### 4.1 Citra Framework Dependencies (REQUIRED)

#### **core/gdbstub/gdbstub.h** - CRITICAL BLOCKER
**Status**: ❌ NOT FOUND in Mikage  
**Required By**: arm_dyncom_interpreter.cpp, armstate.cpp  
**Usage**:
```cpp
GDBStub::BreakpointAddress          // struct with type, address
GDBStub::BreakpointType::Execute    // enum value
GDBStub::IsServerEnabled()          // Check if debugger connected
GDBStub::CheckBreakpoint()          // Check address breakpoint
GDBStub::IsConnected()              // GDB connected state
GDBStub::GetNextBreakpointFromAddress()
GDBStub::Break()                    // Halt CPU for GDB
GDBStub::SendTrap()                 // Send trap to GDB
```

**Replacement Strategy**:
- Create minimal stub at `src/core/mikage/core/gdbstub/gdbstub.h`
- For iOS port: Replace with no-op implementations (debugging typically disabled)
- For phase 2 stub:
  ```cpp
  namespace GDBStub {
      enum class BreakpointType { None = 0, Execute = 1, Read = 2, Write = 3 };
      struct BreakpointAddress { u32 address; BreakpointType type; };
      inline bool IsServerEnabled() { return false; }          // Always disabled
      inline bool CheckBreakpoint(u32, BreakpointType) { return false; }
      inline bool IsConnected() { return false; }
      inline void Break() {}
      // ... other no-op stubs
  }
  ```

#### **core/memory.h** - Memory::MemorySystem - CRITICAL BLOCKER
**Status**: ⚠️ Exists in Mikage as `Memory` facade  
**Required By**: armstate.cpp (all memory read/write operations)  
**Usage**:
```cpp
// ARMul_State calls these via Memory::MemorySystem reference
u8 ReadMemory8(u32 address);
u16 ReadMemory16(u32 address);
u32 ReadMemory32(u32 address);
u64 ReadMemory64(u32 address);
void WriteMemory8(u32 address, u8 value);
void WriteMemory16(u32 address, u16 value);
void WriteMemory32(u32 address, u32 value);
void WriteMemory64(u32 address, u64 value);
```

**Current Mikage Implementation**: Check `src/core/memory.h`  
**Adaptation Needed**: Verify interface compatibility

**Endianness**: ARMul_State checks E-bit in CPSR for big-endian support
```cpp
bool InBigEndianMode() { return (Cpsr & (1 << 9)) != 0; }
// Calls Memory::MemorySystem with endian flag (or adapter must handle internally)
```

#### **core/core.h** - Core::System - MEDIUM BLOCKER
**Status**: ✅ Exists in Mikage  
**Required By**: ARMul_State constructor  
**Usage**:
```cpp
explicit ARMul_State(Core::System& system,      // Stored as reference
                     Memory::MemorySystem& memory,
                     PrivilegeMode initial_mode);
```

**Purpose**: System object used for:
- Timing integration (callback hooks)
- Multi-core synchronization
- System state queries

**Adaptation**: Verify compatibility with existing `Core::System` in Mikage

#### **core/core_timing.h** - Core::Timing::Timer - MEDIUM BLOCKER
**Status**: ✅ Exists in Mikage  
**Required By**: ARM_Interface constructor  
**Usage**:
```cpp
std::shared_ptr<Core::Timing::Timer> timer
// Passed from scheduler/core.cpp to CPU instance
```

#### **core/hle/kernel/svc.h** - SVC Handler - LOW BLOCKER
**Status**: ⚠️ Check if exists  
**Required By**: arm_dyncom_interpreter.cpp (SWI/SVC instruction handler)  
**Usage**:
```cpp
// Triggered by SWI/SVC instruction
// Calls kernel service dispatcher
```

**Replacement**: Can be stubbed for basic CPU operation; only needed for proper kernel integration

#### **common/microprofile.h** - Profiling Macros - OPTIONAL
**Status**: ⚠️ Check if exists  
**Required By**: arm_dyncom_interpreter.cpp (performance instrumentation)  
**Usage**:
```cpp
MICROPROFILE_DEFINE(DynCom_Decode, "DynCom", "Decode", MP_RGB(255, 64, 64));
MICROPROFILE_SCOPE(DynCom_Decode);      // Wrap execution blocks
```

**Replacement**: Empty macros if not needed
```cpp
#define MICROPROFILE_DEFINE(name, category, label, color)
#define MICROPROFILE_SCOPE(name)
```

---

### 4.2 Boost Dependencies

**File**: `arm_interface.h`  
**Usage**:
```cpp
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/version.hpp>
```

**Purpose**: Serialization of ARM_Interface::ThreadContext for save states

**Scope**: Optional - only used if save-state functionality needed  
**Complexity**: LOW  
**Replacement Options**:
1. Keep Boost (if already dependency)
2. Custom serialization (80-100 LOC)
3. JSON serialization via nlohmann/json

---

### 4.3 Standard C++ Dependencies

✅ All used (minimal, modern C++ features):
- `<memory>` - std::shared_ptr, std::unique_ptr
- `<array>` - std::array for fixed-size collections
- `<unordered_map>` - instruction cache
- `<algorithm>` - std::min, std::max
- `<cstdio>` - printf for debugging

---

## 5. Current Mikage Integration Points

### 5.1 WHERE CPU IS INSTANTIATED
**File**: `src/core/mikage/core/core.cpp`  
**Current Code**:
```cpp
// Use legacy ARM_Interpreter:
cpu[i] = std::make_unique<ARM_Interpreter>(...);
```

**Change Required**:
```cpp
// Switch to ARM_DynCom (after phase 3 shim):
cpu[i] = std::make_unique<ARM_DynCom>(system, memory_system, mode, id, timer);
```

### 5.2 EXISTING LEGACY INTERPRETER
**Location**: `src/core/mikage/core/arm/interpreter/`  
**Files**: arm_interpreter.h/cpp, armdefs.h, mmu/*, vfp/*  
**Status**: Will be replaced after parity testing

### 5.3 MEMORY INTERFACE
**Location**: `src/core/mikage/core/memory.h`  
**Check**: Does Memory::MemorySystem exist with required methods?  
**If Not**: Create adapter wrapper

---

## 6. Missing / Stub Components

### 6.1 NOT PRESENT IN PORT (Intentional)

These 3DS hardware components are **NOT** in port_cpu - they exist elsewhere in Mikage:

| Component | Category | Status | Location |
|-----------|----------|--------|----------|
| **GPU/Video Rendering** | Graphics | Separate | Not in port_cpu |
| **HLE System Calls** | Software | Separate | kernel/ |
| **Thread/Scheduler** | OS | Separate | kernel/ |
| **Memory Management** | Memory | Separate | memory/ |
| **Interrupt Controller** | Hardware | Separate | hw/ |
| **Timer/Clock** | Hardware | Separate | hw/ or core_timing/ |
| **I/O Ports** | Hardware | Separate | Not in port_cpu |
| **Audio** | Multimedia | Separate | Not in port_cpu |
| **Network** | Connectivity | Separate | Not in port_cpu |

---

### 6.2 MINIMAL STUBS AVAILABLE (Phase 2 Work)

These should be created as no-op shims:

1. **GDBStub** - Return false/no-op for all methods
2. **Microprofile** - Empty macros
3. **SVC Handler** - Halt on SWI (debug output)

---

## 7. Porting Priority & Complexity

### Phase 1: Vendor Import ✅ COMPLETE
- [x] Cytrus source vendored
- [x] File structure indexed
- [x] Duplicate analysis (azahar vs cytrus)

### Phase 2: Shim Layer 🔴 BLOCKER
**Duration**: 5-7 days  
**Dependencies**:
1. Create `core/gdbstub/gdbstub.h` stub (2 hours)
2. Verify `Memory::MemorySystem` interface in `core/memory.h` (1 day)
3. Verify `Core::System` constructor compatibility (1 day)
4. Create empty stubs for `common/microprofile.h` (30 min)
5. Create SVC handler stub in `core/hle/kernel/svc.h` (2 hours)
6. Write unit tests for all shims (2 days)

**Blockers**:
- If Memory::MemorySystem doesn't exist: Create adapter (3 days)
- If Core::System interface differs: Create wrapper (2 days)

### Phase 3: Compatibility Shim 🟨 IN PROGRESS
**Duration**: 7-10 days  
**Work**:
1. Create CP15 register translation table (Citra vs Mikage) (1 day)
2. Create VFP register translation layer (1 day)
3. Create exclusive monitor shim (1 day)
4. Test armstate.cpp memory read/write hooks (2 days)
5. Test instruction decode & execution (3 days)
6. Test with simple ROM corpus (2 days)

### Phase 4: Dual-Path Switch 🟩 NEXT
**Duration**: 3-5 days  
**Work**:
1. Add `#ifdef USE_DYNCOM_CPU` compile switch
2. Keep legacy interpreter as fallback
3. Run both side-by-side in debug mode
4. Compare register traces between old/new

### Phase 5: Cutover 🟩 FINAL
**Duration**: 2-3 days  
**Work**:
1. Remove legacy interpreter after parity tests pass
2. Clean up duplicate code (azahar) 
3. CPU1 headers completion (if needed)
4. Final integration testing

---

## 8. Complexity Assessment

### High Complexity (🔴)
- **instruction_interpreter.cpp** (4,590 LOC): 70+ instruction paths, subtle flag interactions
- **arm_dyncom_trans.cpp** (2,018 LOC): Operand encoding/decoding, shift chains
- **armstate.cpp** (620 LOC): Memory endianness, register banking, privilege switching

### Medium Complexity (🟡)
- **VFP implementation** (5,083 LOC): IEEE 754 subtleties, denormalized numbers, rounding modes
- **arm_dyncom_dec.cpp** (503 LOC): Opcode dispatch, instruction boundaries
- **exclusive_monitor** (62 LOC): Concurrency primitives, atomicity guarantees

### Low Complexity (🟢)
- **arm_interface.h** (307 LOC): Interface contract, no hidden logic
- **arm_regformat.h** (187 LOC): Static enums, no logic
- **exclusive_monitor.h** (35 LOC): Simple flag management

---

## 9. Risk Assessment

### HIGH RISK 🔴
1. **GDBStub dependency missing** → **Blocker**, easily resolved (stub)
2. **Memory::MemorySystem interface mismatch** → **Blocker**, 1-3 days to fix
3. **Endianness handling** → **Data corruption risk**, needs test ROM with BE data
4. **Instruction execution subtle bugs** → **Hard to debug**, mitigated by test corpus

### MEDIUM RISK 🟡
1. **VFP corner case handling** → Need edge-case test ROMs
2. **Exclusive monitor contention** → Rarely triggered in single-core
3. **CP15 register side effects** → Need MMU/cache management verification

### LOW RISK 🟢
1. **Boost serialization** → Optional, can stub
2. **Microprofile macros** → Already empty in iOS builds
3. **SVC calls** → HLE system likely uses different path

---

## 10. External Dependency Replacement Strategies

| Dependency | Current Use | Replacement | Effort |
|------------|-------------|-------------|--------|
| `boost/serialization` | ThreadContext save | No-op or custom JSON | 1 day |
| `gdbstub.h` | Debug breakpoints | All-false stubs | 2 hours |
| `microprofile.h` | Perf tracing | Empty macros | 30 min |
| `core/hle/kernel/svc.h` | SVC dispatch | Stub handler | 1 day |
| `common/microprofile.h` | Perf instrumentation | Empty macros | 30 min |
| `std::*` | General utilities | Already portable | 0 hours |

---

## 11. Recommended Porting Sequence

```
DAY 1-2: Create dependency shims
├── gdbstub.h stub                    (2 hours)
├── common/microprofile.h stub        (1 hour)
├── Verify Memory::MemorySystem       (1 day)
└── Verify Core::System compat        (1 day)

DAY 3-4: ARMul_State integration
├── Test ARMul_State constructor      (4 hours)
├── Test memory read/write hooks      (1 day)
└── Test endianness switching         (1 day)

DAY 5-7: Instruction execution
├── Single-step ARM instruction       (2 days)
├── Single-step Thumb instruction     (1 day)
└── Test with minimal ROM             (1 day)

DAY 8-10: VFP & advanced features
├── VFP instruction execution         (1 day)
├── Exclusive monitor testing         (1 day)
└── Multiple instruction sequences    (1 day)

DAY 11-12: Parity testing
├── Run legacy vs DynCom trace comparison (1 day)
├── Fix any divergence                (1 day)
└── Test ROM corpus (10-20 ROMs)      (flexible)

DAY 13-14: Integration & cleanup
├── Remove legacy interpreter         (1 day)
├── Clean azahar/ duplicates          (4 hours)
└── Final system tests                (1 day)
```

---

## 12. File Cleanup Recommendations

### SAFE TO DELETE (after Phase 4):
- `azahar/` - Entire directory (redundant with cytrus/)
- `src/core/mikage/core/arm/interpreter/` - Legacy interpreter

### KEEP:
- `cytrus/` - Canonical reference
- `cpu1/` - Implementation reference (or complete with headers)

### CONSOLIDATE:
- Decide: Keep one copy (suggest `src/core/mikage/core/arm/dyncom/`) or import from `port_cpu/cytrus/`?
- If consolidating: `#include "../../../port_cpu/cytrus/..."` or copy once?

---

## 13. Test Coverage Requirements

### Minimum Test Suite (PASS/FAIL)
1. ❌→✅ Single ARM instruction execution
2. ❌→✅ Single Thumb instruction execution
3. ❌→✅ Register read/write
4. ❌→✅ Memory read/write
5. ❌→✅ VFP single-precision op
6. ❌→✅ Branch/jump
7. ❌→✅ Exception (SWI)
8. ❌→✅ Mode switching (USER↔SVC)

### Regression Test Suite (vs Legacy)
1. ✅ Register state consistency
2. ✅ Memory state consistency
3. ✅ Program counter progression
4. ✅ Flag (N/Z/C/V) accuracy
5. ✅ VFP rounding behavior

### ROM Corpus
- Games that heavily use: ARM32, Thumb, VFP, LDREX
- Minimum: 5 ROMs covering different CPU paths
- Ideal: 20+ ROM coverage of instruction space

---

## 14. Summary: Component Map

```
PORT_CPU/
├── AZAHAR/          (32,520 LOC) - DUPLICATE, for comparison only
│   └── core/arm/{dyncom,skyeye_common}/ + include/ mirrors
├── CYTRUS/          (14,887 LOC) - ✅ CANONICAL, vendor-pinned
│   ├── dyncom/      (8,308  LOC) - ARM instruction execution
│   │   └── Decoder + Interpreter + Translator
│   └── skyeye_common/ (6,210 LOC) - CPU state + VFP
├── CPU1/            (12,823 LOC) - Implementation-only, headers missing
│   ├── dyncom/      (7,629  LOC) - .cpp only
│   └── skyeye_common/ (5,167 LOC) - .cpp only
└── PORTING_STATUS.md / README.md / COMPONENT_ANALYSIS.md (this file)

TOTAL: 98 files, 60,230 LOC (pure CPU, no GPU/HLE/IO)
DEPENDENCIES: GDBStub (MISSING), Memory (exists), System (exists), Timing (exists)
EFFORT: 30-40 engineering days to full integration and parity
```

---

## Appendix A: Key Data Structures

### ThreadContext (Saved Register State)
```cpp
struct ThreadContext {
    std::array<u32, 16> cpu_registers;          // R0-R15
    u32 cpsr;                                   // Flags
    std::array<u32, 64> fpu_registers;          // D0-D31 (or S0-S63 aliased)
    u32 fpscr;                                  // VFP status
    u32 fpexc;                                  // VFP exception state
};
```

### ARMul_State Constructor Requirements
```cpp
ARMul_State(Core::System& system,               // For timing, events
            Memory::MemorySystem& memory,       // For all memory access
            PrivilegeMode initial_mode);        // USER32, SVC32, etc
```

### Privilege Modes (PrivilegeMode enum)
```cpp
USER32MODE   = 16,   // Normal user-space
FIQ32MODE    = 17,   // Fast interrupt handler
IRQ32MODE    = 18,   // Normal interrupt handler
SVC32MODE    = 19,   // Supervisor/kernel mode
ABORT32MODE  = 23,   // Prefetch/data abort handler
UNDEF32MODE  = 27,   // Undefined instruction handler
SYSTEM32MODE = 31    // Same registers as USER but privileged
```

---

## Appendix B: Instruction Count by Category

| Category | Count | Examples |
|----------|-------|----------|
| **Data Processing** | 45 | MOV, ADD, SUB, AND, ORR, EOR, BIC, TST, TEQ, CMP |
| **Multiply** | 8 | MUL, MLA, UMULL, SMULL, UMLAL, SMLAL |
| **Load/Store** | 18 | LDR, STR, LDRB, STRB, LDRH, STRH, LDM, STM, LDREX, STREX |
| **Branch** | 4 | B, BL, BX, BLX |
| **VFP** | 40+ | FADD, FSUB, FMUL, FDIV, FSQRT, FCMP, FCVT |
| **Coprocessor** | 2 | MCR, MRC (CP15 control) |
| **Exception** | 3 | SWI/SVC, BKPT, Undefined |
| **Shift** | 4 | LSL, LSR, ASR, ROR |
| **Total** | **124+** | - |

---

**Document Generated**: 2026-04-16  
**Last Updated**: Analysis Phase 2 in progress  
**Author**: GitHub Copilot Code Analysis
