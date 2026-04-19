// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// iOS compatibility port - ARM_DynCom implementation

#include <algorithm>
#include <memory>

#include "arm_dyncom.h"
#include "dyncom/arm_dyncom.h"
#include "dyncom/arm_dyncom_interpreter.h"
#include "dyncom/arm_dyncom_trans.h"
#include "skyeye_common/armstate.h"
#include "../../../common/compat/core/core.h"
#include "../core_timing.h"

namespace Core {

ARM_DynCom::ARM_DynCom(Core::System& system_, Memory::MemorySystem& memory,
                       PrivilegeMode initial_mode, u32 id,
                       std::shared_ptr<Core::Timing::Timer> timer_)
    : ARM_Interface(id, timer_), system(system_), timer(timer_) {
    state = std::make_unique<ARMul_State>(system, memory, initial_mode);
}

ARM_DynCom::~ARM_DynCom() {}

void ARM_DynCom::Run() {
    ExecuteInstructions(std::max<s64>(timer->GetDowncount(), 0));
}

void ARM_DynCom::Step() {
    ExecuteInstructions(1);
}

void ARM_DynCom::ClearInstructionCache() {
    state->instruction_cache.clear();
    trans_cache_buf_top = 0;
}

void ARM_DynCom::InvalidateCacheRange(u32, std::size_t) {
    ClearInstructionCache();
}

void ARM_DynCom::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    ClearInstructionCache();
}

std::shared_ptr<Memory::PageTable> ARM_DynCom::GetPageTable() const {
    return nullptr;
}

void ARM_DynCom::SetPC(u32 pc) {
    state->Reg[15] = pc;
}

u32 ARM_DynCom::GetPC() const {
    return state->Reg[15];
}

u32 ARM_DynCom::GetReg(int index) const {
    return state->Reg[index];
}

void ARM_DynCom::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

u32 ARM_DynCom::GetVFPReg(int index) const {
    return state->ExtReg[index];
}

void ARM_DynCom::SetVFPReg(int index, u32 value) {
    state->ExtReg[index] = value;
}

u32 ARM_DynCom::GetVFPSystemReg(VFPSystemRegister reg) const {
    return state->VFP[reg];
}

void ARM_DynCom::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    state->VFP[reg] = value;
}

u32 ARM_DynCom::GetCPSR() const {
    return state->Cpsr;
}

void ARM_DynCom::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

u32 ARM_DynCom::GetCP15Register(CP15Register reg) const {
    return state->CP15[reg];
}

void ARM_DynCom::SetCP15Register(CP15Register reg, u32 value) {
    state->CP15[reg] = value;
}

void ARM_DynCom::ExecuteInstructions(u64 num_instructions) {
    state->NumInstrsToExecute = num_instructions;
    // Call the dynamic interpreter
    // This is defined in arm_dyncom_interpreter.cpp
    InterpreterMainLoop(state.get());
    timer->AddTicks(num_instructions);
}

} // namespace Core
