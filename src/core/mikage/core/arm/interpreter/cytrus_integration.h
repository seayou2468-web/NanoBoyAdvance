// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/**
 * Cytrus CPU Integration Header
 * 
 * This header provides compatibility between Mikage's legacy ARM interpreter system
 * and the Cytrus DynCom CPU implementation. It adapts Cytrus headers and components
 * to work seamlessly with the existing Mikage architecture.
 */

// Prevent multiple inclusion of Cytrus headers
#ifndef CYTRUS_INTEGRATION_HPP
#define CYTRUS_INTEGRATION_HPP

#include <cstdint>
#include <array>
#include <memory>

// ARMul_State structure compatibility
// The following defines ensure that Cytrus ARMul_State integrates with Mikage
struct ARMul_State;

// VFP system register indices
enum class VFPSystemRegister : unsigned int {
    VFP_FPSID = 0,  // VFP System ID
    VFP_FPSCR = 1,  // VFP Status and Control Register
    VFP_FPEXC = 2,  // VFP Exception Register
    VFP_FPINST = 3, // VFP Instruction
    VFP_FPINST2 = 4,// VFP Instruction 2
};

// Privilege modes
enum class PrivilegeMode : uint32_t {
    USER32MODE = 16,
    FIQ32MODE = 17,
    IRQ32MODE = 18,
    SVC32MODE = 19,
    ABORT32MODE = 23,
    UNDEF32MODE = 27,
    SYSTEM32MODE = 31
};

// CP15 register indices
enum class CP15Register : unsigned int {
    MIDR = 0,                    // Main ID Register
    CLR_INVALIDATE_ALL = 1,      // Cache Level and Type Register
    CTR = 2,                     // Control Type Register
    CCSIDR = 3,                  // Cache Size Selection and ID Register
    // ... add more as needed for Full ARM11 support
    CP15_REG_COUNT = 64
};

// VFP register count definitions  
constexpr unsigned int VFP_SYSTEM_REGISTER_COUNT = 32;
constexpr unsigned int CP15_REGISTER_COUNT = 64;
constexpr unsigned int VFP_FPSCR = static_cast<unsigned int>(VFPSystemRegister::VFP_FPSCR);
constexpr unsigned int VFP_FPEXC = static_cast<unsigned int>(VFPSystemRegister::VFP_FPEXC);
constexpr unsigned int VFP_FPSID = static_cast<unsigned int>(VFPSystemRegister::VFP_FPSID);

/**
 * Adapter class for GDB stub compatibility
 * Enables optional debugging support when GDB stub is available
 */
namespace GDBStub {
    struct BreakpointAddress {
        uint32_t address;
    };
}

#endif // CYTRUS_INTEGRATION_HPP
