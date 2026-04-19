// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "../../common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SVC types

struct MemoryInfo {
    u32 base_address;
    u32 size;
    u32 permission;
    u32 state;
};

struct PageInfo {
    u32 flags;
};

// Extended thread context with full ARM11 state for Cytrus CPU compatibility
struct ThreadContext {
    // General purpose registers (R0-R15)
    std::array<u32, 16> cpu_registers;
    
    // CPSR and SPSR
    u32 cpsr;
    std::array<u32, 7> spsr; // Exception SPSR registers (one per mode)
    
    // VFP registers (32 single-word or 16 double-word)
    std::array<u32, 32> fpu_registers;
    
    // VFP system registers
    u32 fpscr;  // VFP Status and Control Register
    u32 fpexc;  // VFP Exception Register
    u32 fpsid;  // VFP System ID
    
    // CP15 coprocessor registers (essential for full ARM11 support)
    std::array<u32, 64> cp15_registers;
    
    // Extended registers (VFPv3-D32, up to 32 doubles or 64 singles)
    std::array<u32, 64> ext_registers;
    
    // Privilege mode information
    u32 mode;
    u32 bank;
    
    // Flag states
    u32 n_flag;
    u32 z_flag;
    u32 c_flag;
    u32 v_flag;
    u32 q_flag;
    u32 j_flag;
    u32 t_flag; // Thumb state
    
    // Constructor for backwards compatibility
    ThreadContext() {
        cpu_registers.fill(0);
        cpsr = 0;
        spsr.fill(0);
        fpu_registers.fill(0);
        fpscr = 0;
        fpexc = 0;
        fpsid = 0;
        cp15_registers.fill(0);
        ext_registers.fill(0);
        mode = 0;
        bank = 0;
        n_flag = 0;
        z_flag = 0;
        c_flag = 0;
        v_flag = 0;
        q_flag = 0;
        j_flag = 0;
        t_flag = 0;
    }
    
    // Backward compatibility: access old-style packed registers via helper
    u32 GetR13() const { return cpu_registers[13]; }
    void SetR13(u32 val) { cpu_registers[13] = val; }
    
    u32 GetR14() const { return cpu_registers[14]; }
    void SetR14(u32 val) { cpu_registers[14] = val; }
    
    u32 GetPC() const { return cpu_registers[15]; }
    void SetPC(u32 val) { cpu_registers[15] = val; }
};

enum ResetType {
    RESETTYPE_ONESHOT,
    RESETTYPE_STICKY,
    RESETTYPE_PULSE,
    RESETTYPE_MAX_BIT = (1u << 31),
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

namespace SVC {

void Register();

} // namespace
