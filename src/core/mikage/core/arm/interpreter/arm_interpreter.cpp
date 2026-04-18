// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "arm_interpreter.h"
#include "../../../common/common_types.h"
#include "../../memory.h"
#include "cytrus_cpu_bridge.hpp"

// Constructor: Initialize with placeholders - System will be provided later
ARM_Interpreter::ARM_Interpreter() : state(nullptr) {
    // Initialization is deferred until System reference is available via InitializeWithSystem()
    // This maintains backwards compatibility with existing code
}

// Destructor
ARM_Interpreter::~ARM_Interpreter() {
    if (state) {
        delete state;
        state = nullptr;
    }
}

// Initialize the interpreter with System and Memory references
void ARM_Interpreter::InitializeWithSystem(Core::System& system, Memory::MemorySystem& memory) {
    InitializeWithSystem(system, memory, 16); // USER32MODE
}

void ARM_Interpreter::InitializeWithSystem(Core::System& system, Memory::MemorySystem& memory, u32 initial_mode) {
    if (state) {
        delete state;
    }
    
    // Create new Cytrus-compatible ARMul_State with System and Memory integration.
    state = new ARMul_State(system, memory, static_cast<PrivilegeMode>(initial_mode));

    // Configure for ARM11
    state->Emulate = 3;  // RUN mode
    state->Reset();

    // Set up default stack pointer
    state->Reg[13] = 0x10000000;
}

/**
 * Set the Program Counter to an address
 * @param addr Address to set PC to
 */
void ARM_Interpreter::SetPC(u32 pc) {
    if (!state) return;
    state->Reg[15] = pc;
}

/**
 * Get the current Program Counter
 * @return Returns current PC
 */
u32 ARM_Interpreter::GetPC() const {
    if (!state) return 0;
    return state->Reg[15];
}

/**
 * Get an ARM register
 * @param index Register index (0-15)
 * @return Returns the value in the register
 */
u32 ARM_Interpreter::GetReg(int index) const {
    if (!state || index < 0 || index > 15) return 0;
    return state->Reg[index];
}

/**
 * Set an ARM register
 * @param index Register index (0-15)
 * @param value Value to set register to
 */
void ARM_Interpreter::SetReg(int index, u32 value) {
    if (!state || index < 0 || index > 15) return;
    state->Reg[index] = value;
}

/**
 * Get the current CPSR register
 * @return Returns the value of the CPSR register
 */
u32 ARM_Interpreter::GetCPSR() const {
    if (!state) return 0;
    return state->Cpsr;
}

/**
 * Set the current CPSR register
 * @param cpsr Value to set CPSR to
 */
void ARM_Interpreter::SetCPSR(u32 cpsr) {
    if (!state) return;
    state->Cpsr = cpsr;
}

/**
 * Returns the number of clock ticks since the last reset
 * @return Returns number of clock ticks
 */
u64 ARM_Interpreter::GetTicks() const {
    if (!state) return 0;
    return state->NumInstrs;
}

/**
 * Get a CP15 coprocessor register
 */
u32 ARM_Interpreter::GetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2) const {
    if (!state) return 0;
    return state->ReadCP15Register(crn, opcode_1, crm, opcode_2);
}

/**
 * Set a CP15 coprocessor register
 */
void ARM_Interpreter::SetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2, u32 value) {
    if (!state) return;
    state->WriteCP15Register(value, crn, opcode_1, crm, opcode_2);
}

/**
 * Get a VFP register
 */
u32 ARM_Interpreter::GetVFPRegister(int index) const {
    if (!state || index < 0 || index >= 64) return 0;
    return state->ExtReg[index];
}

/**
 * Set a VFP register
 */
void ARM_Interpreter::SetVFPRegister(int index, u32 value) {
    if (!state || index < 0 || index >= 64) return;
    state->ExtReg[index] = value;
}

/**
 * Get VFP status and control register (FPSCR)
 */
u32 ARM_Interpreter::GetVFPSystemReg() const {
    if (!state) return 0;
    return state->VFP[1];  // FPSCR is at index 1
}

/**
 * Set VFP status and control register (FPSCR)
 */
void ARM_Interpreter::SetVFPSystemReg(u32 value) {
    if (!state) return;
    state->VFP[1] = value;  // FPSCR is at index 1
}

/**
 * Get the privilege mode
 */
u32 ARM_Interpreter::GetPrivilegeMode() const {
    if (!state) return 16;  // USER32MODE
    return state->Mode;
}

/**
 * Set the privilege mode
 */
void ARM_Interpreter::SetPrivilegeMode(u32 mode) {
    if (!state) return;
    state->ChangePrivilegeMode(mode);
}

/**
 * Get Thumb flag state
 */
u32 ARM_Interpreter::GetThumbFlag() const {
    if (!state) return 0;
    return state->TFlag;
}

/**
 * Set Thumb flag state
 */
void ARM_Interpreter::SetThumbFlag(u32 value) {
    if (!state) return;
    state->TFlag = (value != 0) ? 1 : 0;
}

/**
 * Executes the given number of instructions
 * @param num_instructions Number of instructions to execute
 */
void ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    if (!state) return;
    
    state->NumInstrsToExecute = num_instructions;
    
    // Call the interpreter main loop
    unsigned int ticks_executed = InterpreterMainLoop(state);
    
    // Update instruction count and accumulate ticks
    state->NumInstrs += ticks_executed;
}

/**
 * Saves the current CPU context
 * @param ctx Thread context to save
 */
void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    if (!state) return;
    
    // Save general purpose registers
    for (int i = 0; i < 16; i++) {
        ctx.cpu_registers[i] = state->Reg[i];
    }
    
    // Save status registers
    ctx.cpsr = state->Cpsr;
    for (int i = 0; i < 7; i++) {
        ctx.spsr[i] = state->Spsr[i];
    }
    
    // Save VFP registers
    for (int i = 0; i < 32; i++) {
        ctx.fpu_registers[i] = state->ExtReg[i];
    }
    ctx.fpscr = state->VFP[1];  // FPSCR
    ctx.fpexc = state->VFP[2];  // FPEXC
    ctx.fpsid = state->VFP[0];  // FPSID
    
    // Save extended registers (remaining 32 for 64-bit VFP total)
    for (int i = 0; i < 64; i++) {
        ctx.ext_registers[i] = state->ExtReg[i];
    }
    
    // Save CP15 registers
    for (int i = 0; i < 64; i++) {
        ctx.cp15_registers[i] = state->CP15[i];
    }
    
    // Save mode and flags
    ctx.mode = state->Mode;
    ctx.bank = state->Bank;
    ctx.n_flag = state->NFlag;
    ctx.z_flag = state->ZFlag;
    ctx.c_flag = state->CFlag;
    ctx.v_flag = state->VFlag;
    ctx.q_flag = state->shifter_carry_out;
    ctx.j_flag = (state->Cpsr >> 24) & 1;
    ctx.t_flag = state->TFlag;
}

/**
 * Loads a CPU context
 * @param ctx Thread context to load
 */
void ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    if (!state) return;
    
    // Load general purpose registers
    for (int i = 0; i < 16; i++) {
        state->Reg[i] = ctx.cpu_registers[i];
    }
    
    // Load status registers
    state->Cpsr = ctx.cpsr;
    for (int i = 0; i < 7; i++) {
        state->Spsr[i] = ctx.spsr[i];
    }
    
    // Load VFP registers
    for (int i = 0; i < 32; i++) {
        state->ExtReg[i] = ctx.fpu_registers[i];
    }
    state->VFP[1] = ctx.fpscr;  // FPSCR
    state->VFP[2] = ctx.fpexc;  // FPEXC
    state->VFP[0] = ctx.fpsid;  // FPSID
    
    // Load extended registers
    for (int i = 0; i < 64; i++) {
        state->ExtReg[i] = ctx.ext_registers[i];
    }
    
    // Load CP15 registers
    for (int i = 0; i < 64; i++) {
        state->CP15[i] = ctx.cp15_registers[i];
    }
    
    // Load mode and flags
    state->Mode = ctx.mode;
    state->Bank = ctx.bank;
    state->NFlag = ctx.n_flag;
    state->ZFlag = ctx.z_flag;
    state->CFlag = ctx.c_flag;
    state->VFlag = ctx.v_flag;
    state->shifter_carry_out = ctx.q_flag;
    state->TFlag = ctx.t_flag;
    if (ctx.j_flag) {
        state->Cpsr |= (1U << 24);
    } else {
        state->Cpsr &= ~(1U << 24);
    }
}
