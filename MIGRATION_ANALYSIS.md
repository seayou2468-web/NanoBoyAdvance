# ARM11 CPU Interpreter Migration Analysis
## NanoBoyAdvance Mikage (3DS Emulator) - Skyeye to Cytrus DynCom

**Project**: NanoBoyAdvance 3DS Emulator (Mikage)
**Scope**: Complete migration from legacy Skyeye-based ARM11 interpreter to vendored Cytrus DynCom interpreter
**Current Architecture**: Old Skyeye ARMulator model (32-bit emulation)
**Target Architecture**: Cytrus DynCom dynamic recompiler with translation caching
**Complexity**: High (~8-10 integration points, significant refactoring needed)

---

## EXECUTIVE SUMMARY

This migration moves from a traditional switch-statement based ARM interpreter (Skyeye ARMulator, ~4000+ LOC) to a higher-performance dynamic recompilation engine (Cytrus DynCom). The migration involves:

1. **Thread initialization refactoring** - Migrate from simple `new ARM_Interpreter()` to context-aware CPU creation
2. **Memory system adaptation** - Wrap existing memory Read/Write calls into Cytrus `MemorySystem` interface
3. **State management rework** - Different CPU state initialization signatures and privilege mode handling
4. **Interface expansion** - Add VFP and CP15 register accessors to legacy ARM_Interface
5. **Cache management** - Introduce instruction cache invalidation callbacks
6. **Thread context alignment** - Unify ThreadContext structure between legacy and new systems

---

## PART 1: CURRENT ARCHITECTURE ANALYSIS

### 1.1 Legacy Interpreter File Structure

**Location**: `src/core/mikage/core/arm/interpreter/`

```
interpreter/
├── arm_interpreter.h       (48 lines) - Derives from ARM_Interface
├── arm_interpreter.cpp     (105 lines) - Factory, basic CPU setup
├── armdefs.h               (596 lines) - Skyeye type defs, privilege modes
├── armemu.h/cpp            (~4700 lines) - Main execution loop (ARMul_Emulate32)
├── armemu_thumb.cpp        (~1000 lines) - Thumb instruction handler
├── armcopro.cpp            (~800 lines) - Coprocessor emulation
├── armmmu.h/cpp            (~800 lines) - MMU operations
├── armos.h/cpp             (~400 lines) - OS syscall stubs
├── armsupp.cpp             (~500 lines) - Support functions
├── armvirt.cpp             (~300 lines) - Virtual memory
├── arminit.cpp             (~500 lines) - CPU initialization
├── mmu/
│   ├── arm1176jzf_s_mmu.h  - ARM1176 MMU definitions
│   ├── cache.h/cpp          - Cache management
│   ├── tlb.h/cpp            - TLB management
│   └── ...                  - Buffer management
├── vfp/
│   └── asm_vfp.h           - VFP (floating point) registers and ops
├── arm_regformat.h         (73 lines) - Register enums (VFP, CP15)
└── skyeye_defs.h
```

**Key Classes/Structures**:
- `ARM_Interpreter` - Wraps ARMul_State, implements ARM_Interface
- `ARMul_State` - CPU state (legacy version, minimal structure)
- Various `ARMul_*` functions - Dispatch and execution helpers

**Core Execution Pattern**:
```cpp
void ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    state->NumInstrsToExecute = num_instructions;
    ARMul_Emulate32(state);  // Main loop - switch-based interpretation
}
```

### 1.2 Cytrus DynCom File Structure

**Location**: `src/core/mikage/port_cpu/cpu1/dyncom/` (actual files to migrate)
**Also available**: `src/core/mikage/port_cpu/cytrus/dyncom/` (already migrated variant)

```
dyncom/
├── arm_dyncom.cpp          (127 lines) - Factory, integration wrapper
├── arm_dyncom.h            (60 lines) - Class definition
├── arm_dyncom_interpreter.h - Dispatch function declarations
├── arm_dyncom_interpreter.cpp (~2200 lines) - Main execution loop
├── arm_dyncom_dec.cpp       (~400 lines) - Instruction decoding
├── arm_dyncom_trans.cpp     (~500 lines) - Code translation/caching
├── arm_dyncom_thumb.cpp     (~600 lines) - Thumb mode support
└── [shared with Cytrus]: skyeye_common/
    ├── armstate.h/cpp       - Enhanced ARMul_State with memory/system refs
    ├── arm_regformat.h      - Register enums
    └── vfp/asm_vfp.h
```

**Key Classes/Structures**:
- `ARM_DynCom` - Implements ARM_Interface from Cytrus
- `ARMul_State` (Cytrus version) - Takes `Core::System&` and `Memory::MemorySystem&`
- `InterpreterMainLoop()` - Dynamic translation and execution

**Core Execution Pattern**:
```cpp
void ARM_DynCom::ExecuteInstructions(u64 num_instructions) {
    state->NumInstrsToExecute = num_instructions;
    const u32 ticks_executed = InterpreterMainLoop(state.get());
    if (timer) timer->AddTicks(ticks_executed);  // Timing integration
    state->ServeBreak();  // GDB breakpoint handling
}
```

### 1.3 Current Integration Point: core.cpp

**File**: `src/core/mikage/core/core.cpp`

```cpp
// Current initialization
int Init() {
    NOTICE_LOG(MASTER_LOG, "initialized OK");
    g_disasm = new ARM_Disasm();
    g_app_core = new ARM_Interpreter();    // ← Legacy
    g_sys_core = new ARM_Interpreter();    // ← Legacy
    return 0;
}

// Current execution loop  
void RunLoop() {
    for (;;) {
        g_app_core->Run(100);              // Uses simple instruction count
        HW::Update();
        Kernel::Reschedule();
    }
}
```

**Problem**: 
- No timing system integration
- No privilege mode handling (assumes user mode)
- No GDB breakpoint support
- Memory system not explicitly passed
- No instruction cache management

---

## PART 2: REQUIRED CHANGES - FILE-BY-FILE MAPPING

### Phase 1: Foundation Layer Adaptation (PRIORITY: CRITICAL)

#### File 1: `src/core/mikage/core/memory.h` 
**Status**: Minor adaptation needed
**Current Role**: Provides `Memory::MemorySystem` facade

**Changes Required**:
- Already wraps `Memory::Read8/16/32/64` and `Memory::Write8/16/32/64`
- **VERIFY**: `Memory::PageTable` structure exists and is compatible
- **ADD**: Inline methods for page table operations if missing
- **NOTE**: This is already well-designed as an adapter!

**Impact**: Low - mainly verification

---

#### File 2: `src/core/mikage/core/hle/svc.h` and `core/hle/kernel/thread.h`
**Status**: Breaking change needed
**Current Role**: Defines ThreadContext structure

**Current Definition**:
```cpp
struct ThreadContext {
    u32 cpu_registers[13];  // R0-R12
    u32 sp;                 // R13
    u32 lr;                 // R14
    u32 pc;                 // R15
    u32 cpsr;
    u32 fpu_registers[32];  // VFP registers
    u32 fpscr;
    u32 fpexc;
};
```

**Cytrus Version** (target):
```cpp
struct ThreadContext {
    u32 GetStackPointer() const { return cpu_registers[13]; }
    void SetStackPointer(u32 value) { cpu_registers[13] = value; }
    u32 GetLinkRegister() const { return cpu_registers[14]; }
    void SetLinkRegister(u32 value) { cpu_registers[14] = value; }
    u32 GetProgramCounter() const { return cpu_registers[15]; }
    void SetProgramCounter(u32 value) { cpu_registers[15] = value; }

    std::array<u32, 16> cpu_registers{};
    u32 cpsr{};
    std::array<u32, 64> fpu_registers{};  // ← 64 instead of 32!
    u32 fpscr{};
    u32 fpexc{};
};
```

**Changes Required**:
```cpp
// OLD: Uses discrete fields
// NEW: Use std::array and helper methods
struct ThreadContext {
    std::array<u32, 16> cpu_registers{};
    u32 cpsr{};
    std::array<u32, 64> fpu_registers{};  // Extended for VFP support
    u32 fpscr{};
    u32 fpexc{};
    
    // Helper accessors for compatibility
    u32 GetStackPointer() const { return cpu_registers[13]; }
    void SetStackPointer(u32 value) { cpu_registers[13] = value; }
    // ... similar for LR, PC
};
```

**Impact**: High - affects thread scheduling, save/restore

**Affected files that will break**:
- `src/core/mikage/core/hle/kernel/thread.cpp` - SaveContext/LoadContext
- `src/core/mikage/core/arm/interpreter/arm_interpreter.cpp` - SaveContext/LoadContext
- Any code using `ctx.sp`, `ctx.lr`, `ctx.pc` directly

---

#### File 3: `src/core/mikage/core/arm/arm_interface.h`
**Status**: Must extend interface
**Current Role**: Abstract base class for CPU implementations

**Current Methods**:
```cpp
class ARM_Interface : NonCopyable {
    virtual void ExecuteInstructions(int num_instructions) = 0;
    virtual void SetPC(u32 addr) = 0;
    virtual u32 GetPC() const = 0;
    virtual u32 GetReg(int index) const = 0;
    virtual void SetReg(int index, u32 value) = 0;
    virtual u32 GetCPSR() const = 0;
    virtual void SetCPSR(u32 cpsr) = 0;
    virtual u64 GetTicks() const = 0;
    virtual void SaveContext(ThreadContext& ctx) = 0;
    virtual void LoadContext(const ThreadContext& ctx) = 0;
};
```

**Cytrus Extensions**:
```cpp
virtual u32 GetVFPReg(int index) const = 0;
virtual void SetVFPReg(int index, u32 value) = 0;
virtual u32 GetVFPSystemReg(VFPSystemRegister reg) const = 0;
virtual void SetVFPSystemReg(VFPSystemRegister reg, u32 value) = 0;
virtual u32 GetCP15Register(CP15Register reg) const = 0;
virtual void SetCP15Register(CP15Register reg, u32 value) = 0;
virtual void ClearInstructionCache() = 0;
virtual void InvalidateCacheRange(u32 start_address, std::size_t length) = 0;
virtual void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) = 0;
virtual void PrepareReschedule() = 0;
```

**Changes Required**:
```cpp
class ARM_Interface : NonCopyable {
public:
    // ... existing methods ...
    
    // NEW: VFP register accessors
    virtual u32 GetVFPReg(int index) const = 0;
    virtual void SetVFPReg(int index, u32 value) = 0;
    
    // NEW: VFP system register accessors
    virtual u32 GetVFPSystemReg(VFPSystemRegister reg) const = 0;
    virtual void SetVFPSystemReg(VFPSystemRegister reg, u32 value) = 0;
    
    // NEW: CP15 register accessors  
    virtual u32 GetCP15Register(CP15Register reg) const = 0;
    virtual void SetCP15Register(CP15Register reg, u32 value) = 0;
    
    // NEW: Cache and memory management
    virtual void ClearInstructionCache() = 0;
    virtual void InvalidateCacheRange(u32 start_address, std::size_t length) = 0;
    
    // NEW: Page table notification (for cache validation)
    virtual void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) = 0;
    
    // NEW: Reschedule notification
    virtual void PrepareReschedule() = 0;
};
```

**Impact**: Medium - legacy ARM_Interpreter class must implement stubs

---

### Phase 2: Legacy Interpreter Updates (PRIORITY: HIGH)

#### File 4: `src/core/mikage/core/arm/interpreter/arm_interpreter.h`
**Status**: Must add stub implementations

**Add Method Implementations**:
```cpp
class ARM_Interpreter : virtual public ARM_Interface {
    // ... existing ...
    
    // NEW stubs for VFP
    u32 GetVFPReg(int index) const override {
        return state->ExtReg[index];  // Check ExtReg exists in old ARMul_State
    }
    void SetVFPReg(int index, u32 value) override {
        state->ExtReg[index] = value;
    }
    
    // NEW stubs for VFP system registers
    u32 GetVFPSystemReg(VFPSystemRegister reg) const override {
        return state->VFP[reg];
    }
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override {
        state->VFP[reg] = value;
    }
    
    // NEW stubs for CP15
    u32 GetCP15Register(CP15Register reg) const override {
        return state->CP15[reg];  // Check CP15 array exists
    }
    void SetCP15Register(CP15Register reg, u32 value) override {
        state->CP15[reg] = value;
    }
    
    // NEW stubs - cache operations (no-ops for interpreter)
    void ClearInstructionCache() override {}
    void InvalidateCacheRange(u32, std::size_t) override {}
    void SetPageTable(const std::shared_ptr<Memory::PageTable>&) override {}
    void PrepareReschedule() override {}
};
```

**Impact**: Low - just stubs

---

#### File 5: `src/core/mikage/core/arm/interpreter/arm_interpreter.cpp`
**Status**: Update SaveContext/LoadContext for new ThreadContext

**Current Code**:
```cpp
void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->Reg, sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->ExtReg, sizeof(ctx.fpu_registers));
    ctx.sp = state->Reg[13];
    ctx.lr = state->Reg[14];
    ctx.pc = state->pc;
    ctx.cpsr = state->Cpsr;
    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];
}
```

**Updated Code**:
```cpp
void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    // Copy all 16 CPU registers
    std::copy(state->Reg, state->Reg + 16, ctx.cpu_registers.begin());
    
    // Copy VFP - extend to 64 if state->ExtReg supports it, else zero-fill
    std::copy(state->ExtReg, state->ExtReg + 32, ctx.fpu_registers.begin());
    std::fill(ctx.fpu_registers.begin() + 32, ctx.fpu_registers.end(), 0);
    
    ctx.cpsr = state->Cpsr;
    ctx.fpscr = state->VFP[VFP_FPSCR];   // Use named constant
    ctx.fpexc = state->VFP[VFP_FPEXC];
}

void ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    std::copy(ctx.cpu_registers.begin(), ctx.cpu_registers.end(), state->Reg);
    std::copy(ctx.fpu_registers.begin(), ctx.fpu_registers.begin() + 32, state->ExtReg);
    
    state->Cpsr = ctx.cpsr;
    state->VFP[VFP_FPSCR] = ctx.fpscr;
    state->VFP[VFP_FPEXC] = ctx.fpexc;
    state->Reg[15] = ctx.cpu_registers[15];  // PC
    state->NextInstr = RESUME;
}
```

**Impact**: Medium - structure compatibility issue

---

### Phase 3: Core System Integration (PRIORITY: CRITICAL)

#### File 6: `src/core/mikage/core/system.h` / `core.h`
**Status**: Add timing system integration

**Current System typedef**:
```cpp
class Core::System {
    // Currently not used by CPU initialization
};
```

**What Cytrus DynCom Requires**:
```cpp
// Needs to be passed to ARM_DynCom constructor
Core::System& system_
```

**Changes in core.h**:
```cpp
namespace Core {
    class Timing::Timer;  // Forward declare
    
    class System {
        std::shared_ptr<Timing::Timer> timer;
        // ... other fields ...
    };
}
```

**Impact**: Medium - system architecture change

---

#### File 7: `src/core/mikage/core/core_timing.h` / `core_timing.cpp`
**Status**: Must exist or be created
**Current State**: Likely exists in Citra/Mikage

**Required Interface**:
```cpp
namespace Core::Timing {
    class Timer {
        s64 GetDowncount() const;  // Read cycle countdown
        void AddTicks(u32 ticks);   // Register executed ticks
    };
}
```

**Purpose**: Allows DynCom to know how many instruction cycles were consumed and update emulation timing

**Impact**: Medium - timing subsystem integration

---

#### File 8: `src/core/mikage/core/core.cpp`
**Status**: Major refactoring required

**Current Init**:
```cpp
int Init() {
    g_disasm = new ARM_Disasm();
    g_app_core = new ARM_Interpreter();  // ← CHANGE
    g_sys_core = new ARM_Interpreter();  // ← CHANGE
    return 0;
}
```

**Required New Init** (Two strategies):

**Strategy A: Keep Legacy Until Cutover**
```cpp
#if USE_CYTRUS_CPU
    #include "./arm/dyncom/arm_dyncom.h"
    #define CPU_CLASS ARM_DynCom
#else
    #include "./arm/interpreter/arm_interpreter.h"
    #define CPU_CLASS ARM_Interpreter
#endif

int Init() {
    Memory::Init();
    Core::System system;  // Create system instance
    
#if USE_CYTRUS_CPU
    auto timer = std::make_shared<Core::Timing::Timer>();
    auto memory = std::make_unique<Memory::MemorySystem>();
    g_app_core = new ARM_DynCom(
        system,
        *memory,
        PrivilegeMode::USER32MODE,  // app core
        0,                            // core ID
        timer
    );
    g_sys_core = new ARM_DynCom(
        system,
        *memory,
        PrivilegeMode::SVC32MODE,   // sys core in supervisor
        1,                            // core ID
        timer
    );
#else
    g_app_core = new ARM_Interpreter();
    g_sys_core = new ARM_Interpreter();
#endif
    return 0;
}
```

**Strategy B: Fully Migrated**
```cpp
int Init() {
    Memory::Init();
    g_system = std::make_unique<Core::System>();
    g_memory = std::make_unique<Memory::MemorySystem>();
    g_timer = std::make_shared<Core::Timing::Timer>();
    
    g_app_core = std::make_unique<ARM_DynCom>(
        *g_system,
        *g_memory,
        USER32MODE,   // Application code runs in user mode
        0,
        g_timer
    );
    g_sys_core = std::make_unique<ARM_DynCom>(
        *g_system,
        *g_memory,
        SVC32MODE,    // System code runs in supervisor mode
        1,
        g_timer
    );
    return 0;
}
```

**Impact**: High - core initialization completely changes

---

### Phase 4: Cytrus DynCom Integration (PRIORITY: HIGH)

#### File 9: Port `src/core/mikage/port_cpu/cpu1/dyncom/arm_dyncom.cpp`
**Status**: Already exists - may need minor path/integration fixes

**What to verify/fix**:
```cpp
// Current includes at top
#include "core/arm/dyncom/arm_dyncom.h"           // ← Check path
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/arm/dyncom/arm_dyncom_trans.h"
#include "core/arm/skyeye_common/armstate.h"      // ← Shared state
#include "core/core.h"                             // ← Core::System, timing
#include "core/core_timing.h"

// Constructor needs to work with mikage paths
ARM_DynCom::ARM_DynCom(Core::System& system_, Memory::MemorySystem& memory,
                       PrivilegeMode initial_mode, u32 id,
                       std::shared_ptr<Core::Timing::Timer> timer)
    : ARM_Interface(id, timer), system(system_) {
    state = std::make_unique<ARMul_State>(system, memory, initial_mode);
}
```

**Changes Needed**:
1. Include paths - may need adjustment for Mikage directory structure
2. Memory::MemorySystem must be available (already created in File 1)
3. PrivilegeMode enum must be accessible
4. Timer interface must match

**Impact**: Low - mostly path/namespace fixes

---

#### File 10: ARMul_State Constructor (port_cpu/cpu1/skyeye_common/armstate.cpp)
**Status**: Must adapt for Mikage memory system

**Cytrus Version Constructor**:
```cpp
ARMul_State::ARMul_State(Core::System& system_, Memory::MemorySystem& memory_,
                         PrivilegeMode initial_mode)
    : system(system_), memory(memory_) {
    ChangePrivilegeMode(initial_mode);
    // ... initialization ...
}
```

**Mikage Adaptation**:
```cpp
// Ensure constructor accepts the right types:
ARMul_State::ARMul_State(Core::System& system_,      // From system.h
                         Memory::MemorySystem& memory_, // From memory.h
                         PrivilegeMode initial_mode)    // From armstate.h
    : system(system_), memory(memory_) {
    ChangePrivilegeMode(initial_mode);
    // Initialize CP15 registers, VFP state, etc.
}
```

**Key Initialization Points**:
```cpp
void ARMul_State::Reset() {
    // Initialize all CPU registers to 0
    std::fill(Reg, Reg + 16, 0);
    
    // Set up CP15 registers for ARM11 (3DS)
    CP15[CP15_MAIN_ID] = 0x412fc083;        // ARM1176 ID
    CP15[CP15_CACHE_TYPE] = 0x0e112012;     // Cache configuration
    CP15[CP15_CONTROL] = 0x00052078;        // MMU enabled, etc.
    
    // VFP initialization
    VFP[VFP_FPSID] = 0x410110A1;
    VFP[VFP_FPEXC] = 0x40000000;
    
    // Initialize instruction cache
    instruction_cache.clear();
}
```

**Impact**: Medium - state initialization critical

---

#### File 11: Instruction Dispatcher (port_cpu/cpu1/dyncom/arm_dyncom_interpreter.cpp)
**Status**: Core implementation - likely works as-is

**Key Function**:
```cpp
unsigned InterpreterMainLoop(ARMul_State* cpu) {
    u32 ticks_executed = 0;
    
    while (cpu->NumInstrsToExecute > 0) {
        // Fetch, decode, translate, execute
        // Return tick count
    }
    
    return ticks_executed;  // ← Timing integration point
}
```

**What needs to work**:
1. Instruction cache must access memory via `cpu->memory`
2. CP15 operations must work (TTBR, control register reads)
3. Privilege mode changes must call `cpu->ChangePrivilegeMode()`
4. Memory read/writes through ARMul_State (not direct)

**Verify**:
- Search for direct memory access - convert to use ARMul_State methods:
  ```cpp
  // OLD (likely in some places):
  u32 data = *(u32*)address;
  
  // NEW:
  u32 data = cpu->ReadMemory32(address);
  ```

**Impact**: Low - should work with Memory::MemorySystem adapter

---

### Phase 5: Memory Integration (PRIORITY: HIGH)

#### File 12: ARM_DynCom Memory Callbacks
**Location**: Likely in `arm_dyncom_interpreter.cpp` or `armstate.cpp`

**Current Legacy Pattern** (Skyeye):
```cpp
// In armemu.cpp or similar
void read_memory_8(ARMul_State *state, ARMword addr) {
    return Memory::Read8(addr);  // Direct global call
}
```

**Cytrus Pattern** (what we need):
```cpp
u8 ARMul_State::ReadMemory8(u32 address) const {
    return memory.Read8(address);  // Instance method on MemorySystem
}
```

**Integration Points to Verify**:
1. All instruction fetches use state->ReadMemory32()
2. All data loads use appropriate ReadMemory* methods
3. All stores use WriteMemory* methods
4. CP15 access to memory (e.g., page table walks) uses proper callbacks

**Existing Adapter** (in src/core/mikage/core/memory.h):
```cpp
class MemorySystem {
    u8 Read8(u32 address) const { return Memory::Read8(address); }
    u16 Read16(u32 address) const { return Memory::Read16(address); }
    u32 Read32(u32 address) const { return Memory::Read32(address); }
    u64 Read64(u32 address) const { /* ... */ }
    void Write8/16/32/64(...) { /* ... */ }
};
```

**Impact**: High - must ensure memory callbacks work correctly

---

#### File 13: Exclusive Monitors (port_cpu/cpu1/exclusive_monitor.h/cpp if needed)
**Status**: Optional - multi-core synchronization

**Currently**:
```cpp
class ExclusiveMonitor {
    virtual u8 ExclusiveRead8(size_t core_index, VAddr addr) = 0;
    virtual bool ExclusiveWrite8(size_t core_index, VAddr vaddr, u8 value) = 0;
    // Similar for 16, 32, 64
};
```

**Usage**: 
- Mikage might need this for dual-core ARM11 (app core + system core)
- If not used, can be stubbed

**Mitigation**: If ExclusiveMonitor doesn't exist, create stub:
```cpp
class ExclusiveMonitor {
    u8 ExclusiveRead8(size_t, u32 address) override {
        return memory->Read8(address);  // No actual exclusivity
    }
    bool ExclusiveWrite8(size_t, u32 address, u8 value) override {
        memory->Write8(address, value);
        return true;  // Always succeeds
    }
};
```

**Impact**: Low if not fully implemented

---

### Phase 6: Supporting Infrastructure (PRIORITY: MEDIUM)

#### File 14: `src/core/mikage/core/hle/kernel/thread.cpp`
**Status**: Update SaveContext/LoadContext calls

**Current Code**:
```cpp
static void SaveContext(ThreadContext& ctx) {
    ctx.cpu_registers[0..12] = ...
    ctx.sp = ...
    ctx.lr = ...
    ctx.pc = ...
    ctx.cpsr = ...
}
```

**After ThreadContext refactor** (to std::array):
```cpp
static void SaveContext(ThreadContext& ctx) {
    for (int i = 0; i < 13; i++) {
        ctx.cpu_registers[i] = ...
    }
    // Use helper getters if needed, or direct array access
    ctx.cpsr = ...
}
```

**Impact**: Low - just array indexing changes

---

#### File 15: GDB Stub Integration (Optional - currently disabled)
**Location**: `src/core/mikage/core/gdbstub/gdbstub.h`

**Current**:
```cpp
inline bool IsServerEnabled() { return false; }  // All disabled
```

**If Enabling**:
```cpp
// Add methods to ARM_Interface if needed:
class ARM_Interface {
    virtual void SetPC(u32 addr) = 0;  // For breakpoint handling
    virtual u32 GetReg(int i) const = 0;
    // GetPC/SetReg already exist
};

// GDB stub can call these during breakpoint handling
```

**Impact**: Very Low - currently disabled

---

## PART 3: INTEGRATION POINTS - DETAILED ANALYSIS

### Integration Point A: CPU Creation and Initialization

**Current Location**: `src/core/mikage/core/core.cpp::Init()`

**Dependency Chain**:
```
Init()
├── Memory::Init()                    [memory.cpp]
├── Create Core::System               [system.h]
├── Create Memory::MemorySystem       [memory.h]
├── Create Core::Timing::Timer        [core_timing.h]
└── Create ARM_DynCom instances
    ├── Pass system reference
    ├── Pass memory reference
    ├── Specify privilege mode
    ├── Specify core ID
    └── Pass timer reference
```

**Timing Constraints**:
- Memory must be initialized first (virtual address space setup)
- System must exist (holds global state, timer)
- Timer must be created before CPUs

**Migration Effort**: High - requires understanding initialization order

---

### Integration Point B: Instruction Execution Loop

**Current Location**: `src/core/mikage/core/core.cpp::RunLoop()`

**Current Pattern**:
```cpp
void RunLoop() {
    for (;;) {
        g_app_core->Run(100);     // Fetch, decode, execute 100 instructions
        HW::Update();
        Kernel::Reschedule();
    }
}
```

**Cytrus Pattern** (uses infinite loop inside Run()):
```
Run() {
    ExecuteInstructions(std::max<s64>(timer->GetDowncount(), 0));
}
```

**Key Difference**:
- Legacy: Returns after N instructions
- Cytrus: Reads timer countdown, executes that many cycles, updates timer

**Migration**: Likely works as-is, but timing behavior will change

---

### Integration Point C: Thread Context Save/Load

**Current Location**: `src/core/mikage/core/hle/kernel/thread.cpp`

**Pattern**:
```cpp
void SaveContext() {
    Core::g_app_core->SaveContext(ctx);  // Global CPU pointer
}

void LoadContext() {
    Core::g_app_core->LoadContext(ctx);
}
```

**Both implementations** (legacy and Cytrus) support this interface.

**Breaking Change**: ThreadContext structure size/layout changes

```cpp
// OLD: 52 bytes (13*4 + 4 + 4 + 4 + 32*4 + 4 + 4)
// NEW: 68 bytes (16*4 + 4 + 64*4 + 4 + 4)
```

**Compatibility Issue**: State files saved with old format won't load with new format

---

### Integration Point D: GDB Stub (if enabled)

**Location**: `src/core/mikage/core/gdbstub/gdbstub.cpp` (not created yet)

**Required CPU Interface**:
```cpp
class ARM_Interface {
    virtual u32 GetReg(int i) const = 0;
    virtual void SetReg(int i, u32 val) = 0;
    virtual u32 GetPC() const = 0;
    virtual void SetPC(u32 addr) = 0;
    virtual u32 GetCPSR() const = 0;
    virtual void SetCPSR(u32 cpsr) = 0;
};
```

**Both implementations support this**, GDB integration is transparent.

---

### Integration Point E: CP15 Coprocessor Operations

**Location**: DynCom interpreter, CP15 dispatch handlers

**Mikage Usage**: TBD - need to search for CP15 references

**Key Register Accesses**:
```cpp
// Translation table base registers (MMU)
u32 ttbr0 = state->GetCP15Register(CP15_TRANSLATION_BASE_TABLE_0);
u32 ttbr1 = state->GetCP15Register(CP15_TRANSLATION_BASE_TABLE_1);

// Control registers
u32 control = state->GetCP15Register(CP15_CONTROL);
// Bits: MMU enable (bit 0), cache (bit 2), etc.

// Fault handling
u32 fsr = state->GetCP15Register(CP15_FAULT_STATUS);
u32 far = state->GetCP15Register(CP15_FAULT_ADDRESS);
```

**Adaptation**: Both interpreter versions support this interface

---

### Integration Point F: VFP (Floating Point) Operations

**Location**: arm_dyncom_interpreter.cpp (VFP instruction handlers)

**Required Accessors**:
```cpp
u32 GetVFPReg(int index) const;
void SetVFPReg(int index, u32 value);
u32 GetVFPSystemReg(VFPSystemRegister reg) const;
void SetVFPSystemReg(VFPSystemRegister reg, u32 value);
```

**VFP System Registers**:
```cpp
enum VFPSystemRegister {
    VFP_FPSID,    // 0 - FP System ID Register
    VFP_FPSCR,    // 1 - FP Status and Control Register
    VFP_FPEXC,    // 2 - FP Exception Register
    VFP_FPINST,   // 3 - FP Instruction Register
    VFP_FPINST2,  // 4 - FP Instruction Register 2
    VFP_MVFR0,    // 5 - Media and VFP Feature Register 0
    VFP_MVFR1     // 6 - Media and VFP Feature Register 1
};
```

**Legacy interpreter**: May not have full support - need to check
**Cytrus version**: Fully supported

**Risk**: Legacy ARM_Interpreter needs stubs for these

---

## PART 4: DEPENDENCIES TO RESOLVE

### Dependency 1: Core::System Class

**Required**:
```cpp
namespace Core {
    class System {
        std::shared_ptr<Timing::Timer> timer;
        // ... other members ...
    };
}
```

**Currently**: May not exist or be minimal
**Action**: Create or verify exists with timer member

---

### Dependency 2: Memory::PageTable Structure

**Required**:
```cpp
namespace Memory {
    struct PageTable {
        // Virtual address to physical address mapping
        // Implementation details TBD
    };
}
```

**Check**: 
- Does `Memory::PageTable` exist?
- Does `Memory::MemorySystem::SetPageTable()` need implementation?

**Impact**: Low if PageTable can be stubbed as void*

---

### Dependency 3: std::array<> and std::move<>

**Required**: C++11+ features
**Check**: Makefile/build config supports -std=c++11 or later

---

### Dependency 4: std::make_unique<>

**Required**: C++14 feature
**Check**: Makefile/build config supports -std=c++14 or later
**Fallback**: Use `new` and `std::unique_ptr(...)` constructor

---

### Dependency 5: Type Definitions

**Check these exist and are compatible**:
- `u8`, `u16`, `u32`, `u64` typedefs
- `s64` signed type
- `VAddr` type (virtual address)
- Endianness handling

---

## PART 5: PRIORITY IMPLEMENTATION ORDER

### PHASE 1: Foundation (Week 1)
1. **ThreadContext Refactor** (File 2)
   - Update `src/core/mikage/core/hle/svc.h`
   - Use `std::array<u32, 16>` for CPU regs, `std::array<u32, 64>` for VFP
   - Update all affected code that accesses fields directly

2. **Memory System Verification** (File 1)
   - Verify `Memory::MemorySystem` wrapper class works
   - Test Read8/16/32/64 and Write* calls
   - Check if PageTable type needs definition

3. **ARM_Interface Extension** (File 3)
   - Add VFP, CP15, cache, page table methods to abstract interface
   - This breaks binary compatibility but enables both implementations

### PHASE 2: Legacy Support (Week 1-2)
4. **Legacy ARM_Interpreter Stubs** (Files 4-5)
   - Implement new abstract methods as empty stubs or wrappers
   - Update SaveContext/LoadContext for new ThreadContext
   - Ensure legacy interpreter still works (for fallback)

5. **Thread Integration** (File 14)
   - Update `src/core/mikage/core/hle/kernel/thread.cpp`
   - Handle new ThreadContext layout (std::array)

### PHASE 3: DynCom Porting (Week 2-3)
6. **ARMul_State Enhancement** (port_cpu/cpu1/skyeye_common/)
   - Ensure ARMul_State constructor takes System& and MemorySystem&
   - Implement memory read/write methods
   - Initialize CP15 and VFP registers correctly

7. **ARM_DynCom Factory** (File 9)
   - Verify all includes and paths work in Mikage context
   - Fix any namespace/import issues

8. **Instruction Dispatcher** (File 11)
   - Verify `InterpreterMainLoop()` works with DynCom
   - Check instruction cache, privilege mode changes
   - Verify timing integration (tick counting)

### PHASE 4: Integration Point (Week 3-4)
9. **Memory Integration** (File 12)
   - Ensure all memory accesses go through ARMul_State methods
   - Verify MemorySystem adapter works with all address ranges

10. **Core System Refactor** (Files 6, 7, 8)
    - Create/verify Core::System with Timer
    - Update core.cpp Init() to create DynCom instances
    - Implement Core::Timing::Timer interface

11. **Core.cpp Initialization** (File 8)
    - Implement new CPU creation pattern
    - Verify RunLoop() works with timer integration
    - Test on actual 3DS ROM

### PHASE 5: Testing & Refinement (Week 4)
12. **GDB Stub (Optional)**
13. **Exclusive Monitors (Optional)**
14. **Performance Testing**
15. **Regression Testing** (verify legacy interpreter still works)

---

## PART 6: RISK ASSESSMENT & MITIGATION

### Risk 1: ThreadContext Structure Breaks Savegame Compatibility
**Severity**: HIGH
**Impact**: Save states from old version won't load in new version

**Mitigation**:
- Implement version check in savegame format
- Provide converter function from old to new ThreadContext
- Document breaking change in release notes

---

### Risk 2: Memory Access Performance
**Severity**: MEDIUM
**Impact**: Virtual address translation costs might impact performance

**Mitigation**:
- Ensure Memory::MemorySystem inlines small operations
- Profile MemorySystem::Read32() vs direct memory access
- Consider inline assembly if needed

---

### Risk 3: CP15 Register Initialization
**Severity**: MEDIUM
**Impact**: Incorrect CP15 values could break MMU or cache behavior

**Mitigation**:
- Cross-check with real ARM1176 specs
- Verify against existing 3DS emulators (Citra reference)
- Unit test CP15 read/write operations

---

### Risk 4: Privilege Mode Handling
**Severity**: MEDIUM
**Impact**: Incorrect mode switches could cause crashes or security issues

**Mitigation**:
- Unit test mode switching
- Verify against GDB stub (if enabled)
- Test different privilege modes in actual games

---

### Risk 5: Instruction Cache Invalidation
**Severity**: MEDIUM
**Impact**: Stale cached code could execute

**Mitigation**:
- Ensure SetPageTable() properly clears cache
- Ensure InvalidateCacheRange() works correctly
- Add cache consistency tests

---

### Risk 6: Timer Integration Subtle Bugs
**Severity**: HIGH
**Impact**: Timing bugs could cause desynchronization with hardware

**Mitigation**:
- Profile CPU cycle counting
- Compare with hardware scheduler
- Test on games that depend on timing (music sync, etc.)

---

## PART 7: FILE MODIFICATION CHECKLIST

### FILES TO CREATE
- [ ] `src/core/mikage/core/core_timing.h` (if not exists)
- [ ] `src/core/mikage/core_timing.cpp` (if not exists)

### FILES TO SIGNIFICANTLY MODIFY
- [ ] `src/core/mikage/core/hle/svc.h` - ThreadContext struct
- [ ] `src/core/mikage/core/arm/arm_interface.h` - Extend interface
- [ ] `src/core/mikage/core/core.cpp` - Complete refactor
- [ ] `src/core/mikage/core/system.h` - Add timer field
- [ ] `src/core/mikage/core/arm/interpreter/arm_interpreter.h` - Add stubs
- [ ] `src/core/mikage/core/arm/interpreter/arm_interpreter.cpp` - SaveContext/LoadContext
- [ ] `src/core/mikage/core/hle/kernel/thread.cpp` - ThreadContext handling

### FILES TO COPY/PORT
- [ ] Copy `src/core/mikage/port_cpu/cpu1/dyncom/*` → `src/core/mikage/core/arm/dyncom/`
- [ ] Copy `src/core/mikage/port_cpu/cpu1/skyeye_common/*` → updated versions

### FILES TO VERIFY/MINOR CHANGES
- [ ] `src/core/mikage/core/memory.h` - Ensure MemorySystem interface complete
- [ ] `src/core/mikage/core/arm/interpreter/arm_regformat.h` - Ensure enums match
- [ ] Makefile - Add DynCom source files to build

---

## PART 8: CROSS-REFERENCE: LEGACY VS CYTRUS INTERFACES

| Feature | Legacy ARM_Interpreter | Cytrus ARM_DynCom | Status |
|---------|------------------------|-------------------|--------|
| **GetPC/SetPC** | ✓ Reg[15] | ✓ Reg[15] | Compatible |
| **GetReg/SetReg** | ✓ Basic | ✓ Basic | Compatible |
| **GetCPSR/SetCPSR** | ✓ Basic | ✓ Basic | Compatible |
| **GetVFPReg/SetVFPReg** | ✗ Need to add | ✓ ExtReg[] | **ADD stubs** |
| **GetVFPSystemReg** | ✗ Need to add | ✓ VFP[] array | **ADD stubs** |
| **GetCP15Register** | ✗ Need to add | ✓ CP15[] array | **ADD stubs** |
| **SetCP15Register** | ✗ Need to add | ✓ CP15[] array | **ADD stubs** |
| **SaveContext/LoadContext** | ✓ But different format | ✓ New format | **UPDATE structure** |
| **GetTicks** | ✓ ARMul_Time() | ✓ Timer integration | Compatible |
| **ClearInstructionCache** | ✗ (N/A for interpreter) | ✓ Needed | **ADD stub** |
| **InvalidateCacheRange** | ✗ (N/A for interpreter) | ✓ Needed | **ADD stub** |
| **SetPageTable** | ✗ (N/A for interpreter) | ✓ Needed | **ADD stub** |
| **PrepareReschedule** | ✗ | ✓ Needed | **ADD stub** |
| **Execution Model** | Switch-based loop | Dynamic translation | Different internally |
| **Memory Access** | Global Memory:: calls | ARMul_State methods | **Adapter needed** |
| **System Reference** | None | Core::System& | **New parameter** |
| **Timing Integration** | Counted after exec | Counted inside loop | **Different pattern** |

---

## PART 9: EXAMPLE CODE TEMPLATES

### Template 1: New ARM_Interpreter Stub Methods

```cpp
// In arm_interpreter.h
u32 ARM_Interpreter::GetVFPReg(int index) const {
    if (index < 0 || index >= 32) return 0;
    return state->ExtReg[index];
}

void ARM_Interpreter::SetVFPReg(int index, u32 value) {
    if (index >= 0 && index < 32) {
        state->ExtReg[index] = value;
    }
}

u32 ARM_Interpreter::GetVFPSystemReg(VFPSystemRegister reg) const {
    if (reg >= VFP_SYSTEM_REGISTER_COUNT) return 0;
    return state->VFP[reg];
}

void ARM_Interpreter::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    if (reg < VFP_SYSTEM_REGISTER_COUNT) {
        state->VFP[reg] = value;
    }
}

u32 ARM_Interpreter::GetCP15Register(CP15Register reg) const {
    return state->CP15[reg];
}

void ARM_Interpreter::SetCP15Register(CP15Register reg, u32 value) {
    state->CP15[reg] = value;
}

void ARM_Interpreter::ClearInstructionCache() {
    // Legacy interpreter doesn't cache instructions
}

void ARM_Interpreter::InvalidateCacheRange(u32, std::size_t) {
    // Legacy interpreter doesn't cache instructions
}

void ARM_Interpreter::SetPageTable(const std::shared_ptr<Memory::PageTable>&) {
    // Legacy interpreter doesn't use page tables
}

void ARM_Interpreter::PrepareReschedule() {
    // Legacy interpreter doesn't batch reschedule prep
}
```

### Template 2: New Core.cpp Initialization

```cpp
// In core.cpp

#include "system.h"
#include "core_timing.h"
#include "memory.h"  // MemorySystem
#include "arm/dyncom/arm_dyncom.h"  // After porting

namespace Core {

std::unique_ptr<Core::System> g_system;
std::unique_ptr<Memory::MemorySystem> g_memory;
std::shared_ptr<Core::Timing::Timer> g_timer;

// Existing globals
ARM_Disasm* g_disasm = NULL;
ARM_Interface* g_app_core = NULL;
ARM_Interface* g_sys_core = NULL;

int Init() {
    NOTICE_LOG(MASTER_LOG, "initializing...");
    
    // Initialize memory first
    Memory::Init();
    
    // Create system and timing infrastructure
    g_system = std::make_unique<Core::System>();
    g_memory = std::make_unique<Memory::MemorySystem>();
    g_timer = std::make_shared<Core::Timing::Timer>();
    
    // Create disassembler
    g_disasm = new ARM_Disasm();
    
    // Create CPU cores with DynCom
    g_app_core = new ARM_DynCom(
        *g_system,
        *g_memory,
        USER32MODE,      // Application core runs in user mode
        0,               // Core 0
        g_timer
    );
    
    g_sys_core = new ARM_DynCom(
        *g_system,
        *g_memory,
        SVC32MODE,       // System core runs in supervisor mode
        1,               // Core 1
        g_timer
    );
    
    NOTICE_LOG(MASTER_LOG, "initialized OK");
    return 0;
}

void Shutdown() {
    delete g_disasm;
    delete g_app_core;
    delete g_sys_core;
    
    g_timer.reset();
    g_memory.reset();
    g_system.reset();
    
    Memory::Shutdown();
    NOTICE_LOG(MASTER_LOG, "shutdown OK");
}

} // namespace Core
```

### Template 3: New ThreadContext Definition

```cpp
// In hle/svc.h

#pragma once

#include "../../common/common_types.h"
#include <array>

struct ThreadContext {
    // Accessors for compatibility
    u32 GetStackPointer() const { return cpu_registers[13]; }
    void SetStackPointer(u32 val) { cpu_registers[13] = val; }
    
    u32 GetLinkRegister() const { return cpu_registers[14]; }
    void SetLinkRegister(u32 val) { cpu_registers[14] = val; }
    
    u32 GetProgramCounter() const { return cpu_registers[15]; }
    void SetProgramCounter(u32 val) { cpu_registers[15] = val; }
    
    // CPU state
    std::array<u32, 16> cpu_registers{};  // R0-R15 (PC in R15)
    u32 cpsr{};                            // Current Program Status Register
    
    // Floating point state (VFP)
    std::array<u32, 64> fpu_registers{};   // VFP registers D0-D31 (64-bit each, stored as 2xU32)
    u32 fpscr{};                           // FP Status and Control Register
    u32 fpexc{};                           // FP Exception Register
};
```

---

## PART 10: VALIDATION CHECKLIST

### Pre-Migration Verification
- [ ] All required headers exist and compile (armstate.h, arm_regformat.h, etc.)
- [ ] Memory namespace has Read/Write functions
- [ ] CP15Register and VFPSystemRegister enums exist
- [ ] PrivilegeMode enum defined correctly
- [ ] Build system can compile with C++14 features

### Post-Migration Unit Tests
- [ ] ARM_Interface abstract methods all implemented
- [ ] ThreadContext old format converts to new format correctly
- [ ] Memory::MemorySystem Read/Write work for all address ranges
- [ ] CPU creation with System and MemorySystem references works
- [ ] Timer integration: GetDowncount() and AddTicks() called correctly
- [ ] Privilege mode switches trigger ChangePrivilegeMode()
- [ ] SaveContext/LoadContext preserves CPU state
- [ ] GDB stub (if enabled) can read/write registers

### Integration Tests
- [ ] Simple 3DS ROM loads and starts
- [ ] CPU executes instructions without crashing
- [ ] Game logic doesn't immediately fail
- [ ] Performance comparable or better than legacy

### Regression Tests
- [ ] Legacy ARM_Interpreter still works (if kept as fallback)
- [ ] Thread scheduling hasn't broken
- [ ] Memory access still works correctly
- [ ] Hardware devices still respond to CPU accesses

---

## CONCLUSION

This migration is a significant architectural change affecting:
- **~1500+ lines** of new interface code and stubs
- **~8-10 integration points** requiring careful coordination
- **Timing behavior change** from cycle-counted to tick-based
- **ThreadContext structure break** for save state compatibility

**Expected Outcome**:
- Higher performance through dynamic recompilation
- Better ARM11 compliance through Cytrus implementation
- Groundwork for future multi-core ARM cores
- More complete CP15/VFP register support

**Timeline**: 3-4 weeks for complete migration + testing

**Risk Level**: HIGH - affects core CPU execution, requires thorough testing
