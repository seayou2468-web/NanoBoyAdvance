#include "../../include/cpu_interpreter.hpp"

#include "../../include/kernel/kernel.hpp"
#include "CPU/dyncom/arm_dyncom_interpreter.h"
#include "CPU/skyeye_common/armstate.h"

CPU::CPU(Memory& mem, Emulator& emu, Scheduler& schedulerRef)
    : mem(mem), scheduler(&schedulerRef), emu(emu) {
    reset();
}

CPU::~CPU() = default;

void CPU::reset() {
    gprs.fill(0);
    extRegs.fill(0);
    cpsr = CPSR::UserMode;
    fpscr = FPSCR::MainThreadDefault;
    tlsBase = 0;

    dyncomState = std::make_unique<ARMul_State>(mem, SVC32MODE);
    dyncomState->add_ticks_callback = [this](u64 ticks) {
        if (scheduler != nullptr) {
            scheduler->currentTimestamp += ticks;
        }
    };
    dyncomState->svc_callback = [this](u32 svc) {
        if (kernel == nullptr) {
            Helpers::panic("CPU kernel is not bound");
        }
        kernel->serviceSVC(svc);
    };
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
    if (scheduler == nullptr) {
        Helpers::panic("CPU scheduler is not bound");
    }

    dyncomState->Reg = gprs;
    dyncomState->ExtReg = extRegs;
    dyncomState->Cpsr = cpsr;
    dyncomState->VFP[VFP_FPSCR] = fpscr;
    dyncomState->CP15[CP15_THREAD_UPRW] = tlsBase;

    const u64 budget = (scheduler->nextTimestamp > scheduler->currentTimestamp)
                           ? (scheduler->nextTimestamp - scheduler->currentTimestamp)
                           : 1;
    dyncomState->NumInstrsToExecute = budget;
    const u32 executed = InterpreterMainLoop(dyncomState.get());
    scheduler->currentTimestamp += executed;

    gprs = dyncomState->Reg;
    extRegs = dyncomState->ExtReg;
    cpsr = dyncomState->Cpsr;
    fpscr = dyncomState->VFP[VFP_FPSCR];
    tlsBase = dyncomState->CP15[CP15_THREAD_UPRW];
}

u64 CPU::getTicks() {
    if (scheduler == nullptr) {
        Helpers::panic("CPU scheduler is not bound");
    }
    return scheduler->currentTimestamp;
}

u64& CPU::getTicksRef() {
    if (scheduler == nullptr) {
        Helpers::panic("CPU scheduler is not bound");
    }
    return scheduler->currentTimestamp;
}

Scheduler& CPU::getScheduler() {
    if (scheduler == nullptr) {
        Helpers::panic("CPU scheduler is not bound");
    }
    return *scheduler;
}

void CPU::addTicks(u64 ticks) {
    if (scheduler == nullptr) {
        Helpers::panic("CPU scheduler is not bound");
    }
    scheduler->currentTimestamp += ticks;
}

void CPU::reportMMUFault(u32 fsr, u32 far, bool instruction_fault) {
    const u32 old_pc = gprs[15];
    const u32 vector = instruction_fault ? 0x0C : 0x10;  // Prefetch abort / Data abort vectors

    if (instruction_fault) {
        cp15IFSR = fsr;
        cp15IFAR = far;
        if (dyncomState) {
            dyncomState->CP15[CP15_INSTR_FAULT_STATUS] = fsr;
            dyncomState->CP15[CP15_IFAR] = far;
        }
    } else {
        cp15DFSR = fsr;
        cp15DFAR = far;
        if (dyncomState) {
            dyncomState->CP15[CP15_FAULT_STATUS] = fsr;
            dyncomState->CP15[CP15_FAULT_ADDRESS] = far;
        }
    }

    // Approximate ARM abort exception semantics: switch to Abort mode, mask IRQ, clear Thumb, branch to abort vector.
    cpsr = (cpsr & ~0x1Fu) | CPSR::AbortMode;
    cpsr |= CPSR::IRQDisable;
    cpsr &= ~CPSR::Thumb;
    gprs[14] = old_pc;
    gprs[15] = vector;

    if (dyncomState) {
        dyncomState->Cpsr = cpsr;
        dyncomState->Reg[14] = gprs[14];
        dyncomState->Reg[15] = gprs[15];
    }
}
