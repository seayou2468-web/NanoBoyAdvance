// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "arm_interpreter.h"
#include "../../../common/common_types.h"
#include "../../memory.h"
#include "cytrus_cpu_bridge.hpp"

Core::ARM_Interpreter::ARM_Interpreter() : state(nullptr) {}

Core::ARM_Interpreter::~ARM_Interpreter() {
    delete state;
}

void Core::ARM_Interpreter::InitializeWithSystem(Core::System& system, Memory::MemorySystem& memory) {
    delete state;
    state = new ARMul_State(system, memory, PrivilegeMode::USER32MODE);
    state->Emulate = 3;  // RUN mode
    state->Reset();
    state->Reg[13] = 0x10000000;
}

void Core::ARM_Interpreter::SetPC(u32 pc) {
    if (state) state->Reg[15] = pc;
}

u32 Core::ARM_Interpreter::GetPC() const {
    return state ? state->Reg[15] : 0;
}

u32 Core::ARM_Interpreter::GetReg(int index) const {
    return (state && index >= 0 && index < 16) ? state->Reg[index] : 0;
}

void Core::ARM_Interpreter::SetReg(int index, u32 value) {
    if (state && index >= 0 && index < 16) state->Reg[index] = value;
}

u32 Core::ARM_Interpreter::GetCPSR() const {
    return state ? state->Cpsr : 0;
}

void Core::ARM_Interpreter::SetCPSR(u32 cpsr) {
    if (state) state->Cpsr = cpsr;
}

u64 Core::ARM_Interpreter::GetTicks() const {
    return state ? state->NumInstrs : 0;
}

u32 Core::ARM_Interpreter::GetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2) const {
    return state ? state->ReadCP15Register(crn, opcode_1, crm, opcode_2) : 0;
}

void Core::ARM_Interpreter::SetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2, u32 value) {
    if (state) state->WriteCP15Register(value, crn, opcode_1, crm, opcode_2);
}

u32 Core::ARM_Interpreter::GetVFPRegister(int index) const {
    return (state && index >= 0 && index < 64) ? state->ExtReg[index] : 0;
}

void Core::ARM_Interpreter::SetVFPRegister(int index, u32 value) {
    if (state && index >= 0 && index < 64) state->ExtReg[index] = value;
}

u32 Core::ARM_Interpreter::GetVFPSystemReg() const {
    return state ? state->VFP[1] : 0;
}

void Core::ARM_Interpreter::SetVFPSystemReg(u32 value) {
    if (state) state->VFP[1] = value;
}

u32 Core::ARM_Interpreter::GetPrivilegeMode() const {
    return state ? state->Mode : 16;
}

void Core::ARM_Interpreter::SetPrivilegeMode(u32 mode) {
    if (state) state->ChangePrivilegeMode(mode);
}

u32 Core::ARM_Interpreter::GetThumbFlag() const {
    return state ? state->TFlag : 0;
}

void Core::ARM_Interpreter::SetThumbFlag(u32 value) {
    if (state) state->TFlag = (value != 0);
}

void Core::ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    if (!state) return;
    state->NumInstrsToExecute = num_instructions;
    unsigned int ticks_executed = InterpreterMainLoop(state);
    state->NumInstrs += ticks_executed;
}

void Core::ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    if (!state) return;
    for (int i = 0; i < 16; i++) ctx.cpu_registers[i] = state->Reg[i];
    ctx.cpsr = state->Cpsr;
    for (int i = 0; i < 7; i++) ctx.spsr[i] = state->Spsr[i];
    for (int i = 0; i < 32; i++) ctx.fpu_registers[i] = state->ExtReg[i];
    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];
    ctx.fpsid = state->VFP[0];
    for (int i = 0; i < 64; i++) ctx.ext_registers[i] = state->ExtReg[i];
    for (int i = 0; i < 64; i++) ctx.cp15_registers[i] = state->CP15[i];
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

void Core::ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    if (!state) return;
    for (int i = 0; i < 16; i++) state->Reg[i] = ctx.cpu_registers[i];
    state->Cpsr = ctx.cpsr;
    for (int i = 0; i < 7; i++) state->Spsr[i] = ctx.spsr[i];
    for (int i = 0; i < 64; i++) state->ExtReg[i] = ctx.ext_registers[i];
    state->VFP[1] = ctx.fpscr;
    state->VFP[2] = ctx.fpexc;
    state->VFP[0] = ctx.fpsid;
    for (int i = 0; i < 64; i++) state->CP15[i] = ctx.cp15_registers[i];
    state->Mode = ctx.mode;
    state->Bank = ctx.bank;
    state->NFlag = ctx.n_flag;
    state->ZFlag = ctx.z_flag;
    state->CFlag = ctx.c_flag;
    state->VFlag = ctx.v_flag;
    state->shifter_carry_out = ctx.q_flag;
    state->TFlag = ctx.t_flag;
}
