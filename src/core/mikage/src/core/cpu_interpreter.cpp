#include "../../include/cpu_interpreter.hpp"

#include "../../include/kernel/kernel.hpp"
#include "CPU/dyncom/arm_dyncom_interpreter.h"
#include "CPU/skyeye_common/armstate.h"

CPU::CPU(Memory& mem, Kernel& kernel, Emulator& emu)
    : mem(mem), scheduler(kernel.getScheduler()), kernel(kernel), emu(emu) {
    reset();
}

void CPU::reset() {
    gprs.fill(0);
    extRegs.fill(0);
    cpsr = CPSR::UserMode;
    fpscr = FPSCR::MainThreadDefault;
    tlsBase = 0;

    dyncomState = std::make_unique<ARMul_State>(mem, SVC32MODE);
    dyncomState->add_ticks_callback = [this](u64 ticks) { scheduler.currentTimestamp += ticks; };
    dyncomState->svc_callback = [this](u32 svc) { kernel.serviceSVC(svc); };
    dyncomState->VFP[VFP_FPSCR] = fpscr;
    dyncomState->CP15[CP15_THREAD_UPRW] = tlsBase;
}

void CPU::clearExclusive() {
    if (dyncomState) dyncomState->UnsetExclusiveMemoryAddress();
}

void CPU::loadNZCVT() {
    NFlag = (cpsr >> 31) & 1;
    ZFlag = (cpsr >> 30) & 1;
    CFlag = (cpsr >> 29) & 1;
    VFlag = (cpsr >> 28) & 1;
    TFlag = (cpsr >> 5) & 1;
}

void CPU::saveNZCVT() {
    cpsr &= ~(0xF0000000u | (1u << 5));
    cpsr |= (NFlag & 1) << 31;
    cpsr |= (ZFlag & 1) << 30;
    cpsr |= (CFlag & 1) << 29;
    cpsr |= (VFlag & 1) << 28;
    cpsr |= (TFlag & 1) << 5;
}

void CPU::runFrame() {
    if (!dyncomState) return;

    dyncomState->Reg = gprs;
    dyncomState->ExtReg = extRegs;
    dyncomState->Cpsr = cpsr;
    dyncomState->VFP[VFP_FPSCR] = fpscr;
    dyncomState->CP15[CP15_THREAD_UPRW] = tlsBase;

    const u64 budget = (scheduler.nextTimestamp > scheduler.currentTimestamp)
                           ? (scheduler.nextTimestamp - scheduler.currentTimestamp)
                           : 1;
    dyncomState->NumInstrsToExecute = budget;
    const u32 executed = InterpreterMainLoop(dyncomState.get());
    scheduler.currentTimestamp += executed;

    gprs = dyncomState->Reg;
    extRegs = dyncomState->ExtReg;
    cpsr = dyncomState->Cpsr;
    fpscr = dyncomState->VFP[VFP_FPSCR];
    tlsBase = dyncomState->CP15[CP15_THREAD_UPRW];
}
