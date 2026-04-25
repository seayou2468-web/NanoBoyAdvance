#include "../../../include/cpu_dyncom.hpp"

#include <limits>
#include <utility>

#include "../../../include/kernel/kernel.hpp"
#include "./dyncom/arm_dyncom_interpreter.h"
#include "./skyeye_common/armstate.h"

CPU::CPU(Memory& mem, Scheduler& schedulerRef, std::function<void()> pollSchedulerCallback,
         std::function<bool()> isFrameDoneCallback, std::function<void(bool)> setFrameDoneCallback)
    : mem(mem),
      scheduler(&schedulerRef),
      pollSchedulerCallback(std::move(pollSchedulerCallback)),
      isFrameDoneCallback(std::move(isFrameDoneCallback)),
      setFrameDoneCallback(std::move(setFrameDoneCallback)) {
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
    const u32 current_pc = gprs[15] & ~1u;
    if (mem.getReadPointer(current_pc) == nullptr) {
        reportMMUFault(0x5, current_pc, true);
        return;
    }

    dyncomState->Reg = gprs;
    dyncomState->ExtReg = extRegs;
    dyncomState->Cpsr = cpsr;
    dyncomState->VFP[VFP_FPSCR] = fpscr;
    dyncomState->CP15[CP15_THREAD_UPRW] = tlsBase;

    if (!pollSchedulerCallback || !isFrameDoneCallback || !setFrameDoneCallback) {
        Helpers::panic("CPU frame callbacks are not bound");
    }

    setFrameDoneCallback(false);

    // Execute in scheduler-sized slices until we hit VBlank.
    // This mirrors the old Panda3DS frame pacing model and guarantees that
    // pollScheduler() runs so GPU/HLE interrupts are delivered.
    u32 safety_counter = 0;
    u32 no_progress_counter = 0;
    while (!isFrameDoneCallback()) {
        const u64 budget = (scheduler->nextTimestamp > scheduler->currentTimestamp)
                               ? (scheduler->nextTimestamp - scheduler->currentTimestamp)
                               : 1;
        dyncomState->NumInstrsToExecute = budget;

        const u64 ticks_before = scheduler->currentTimestamp;
        const u32 executed = InterpreterMainLoop(dyncomState.get());
        const u64 ticks_after = scheduler->currentTimestamp;

        // Some dyncom paths may not invoke add_ticks_callback consistently.
        // In that case, fall back to the interpreter's executed count.
        if (ticks_after == ticks_before && executed != 0) {
            scheduler->currentTimestamp += executed;
            no_progress_counter = 0;
        } else if (ticks_after == ticks_before && executed == 0) {
            no_progress_counter++;

            // Dyncom can yield without retiring instructions (eg idle/waiting
            // states). If we do not advance scheduler time here, runFrame can
            // spin forever and never reach the queued VBlank event.
            const u64 next = scheduler->nextTimestamp;
            const u64 panicTimestamp = std::numeric_limits<u64>::max();
            if (next > scheduler->currentTimestamp && next != panicTimestamp) {
                scheduler->currentTimestamp = next;
            } else {
                scheduler->currentTimestamp += 1;
            }
        } else {
            no_progress_counter = 0;
        }

        pollSchedulerCallback();

        if (no_progress_counter > 2048) {
            Helpers::warn("CPU::runFrame prolonged no-progress state (pc=%08X, ticks=%llu, next=%llu)",
                          dyncomState->Reg[15], scheduler->currentTimestamp, scheduler->nextTimestamp);
            no_progress_counter = 0;
        }

        // Avoid infinite loops if the guest gets stuck before scheduling VBlank.
        if (++safety_counter > 200000) {
            Helpers::warn("CPU::runFrame safety break: no VBlank after %u slices (pc=%08X, ticks=%llu, next=%llu)",
                          safety_counter, dyncomState->Reg[15], scheduler->currentTimestamp, scheduler->nextTimestamp);
            break;
        }
    }

    gprs = dyncomState->Reg;
    extRegs = dyncomState->ExtReg;
    cpsr = dyncomState->Cpsr;
    fpscr = dyncomState->VFP[VFP_FPSCR];
    tlsBase = dyncomState->CP15[CP15_THREAD_UPRW];
    finalizeAbortReturnIfNeeded();
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
    const u32 old_cpsr = cpsr;
    const u32 old_pc = gprs[15];
    const bool was_thumb = (old_cpsr & CPSR::Thumb) != 0;
    const u32 vector = instruction_fault ? 0x0C : 0x10;  // Prefetch abort / Data abort vectors
    lastFaultWasInstruction = instruction_fault;
    lastAbortReturnAdjust = instruction_fault ? (was_thumb ? 2u : 4u) : 8u;
    const u32 lr_abort = old_pc + lastAbortReturnAdjust;
    abortReturnPending = true;

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
    gprs[14] = lr_abort;
    gprs[15] = vector;

    if (dyncomState) {
        dyncomState->Reg_abort[0] = gprs[13];
        dyncomState->Reg_abort[1] = lr_abort;
        dyncomState->Spsr[ABORTBANK] = old_cpsr;
        dyncomState->Cpsr = cpsr;
        dyncomState->Reg[14] = gprs[14];
        dyncomState->Reg[15] = gprs[15];
        dyncomState->NumInstrsToExecute = 0;
    }
}

void CPU::finalizeAbortReturnIfNeeded() {
    if (!abortReturnPending) {
        return;
    }

    const u32 mode = cpsr & 0x1Fu;
    if (mode == CPSR::AbortMode) {
        return;
    }

    const u32 lr = gprs[14];
    const u32 pc = gprs[15];

    // Resolve return-instruction pattern:
    //  - MOVS   PC, LR        -> PC == LR
    //  - SUBS   PC, LR, #imm  -> PC == LR - imm
    // For abort handlers, imm is typically 4 (prefetch) or 8 (data).
    auto apply_pc = [&](u32 value, u32 adjust, const char* reason) {
        if (pc != value) {
            Helpers::warn("Adjusting abort return PC (%s): %08X -> %08X", reason, pc, value);
            gprs[15] = value;
            if (dyncomState) {
                dyncomState->Reg[15] = value;
            }
        }
        lastAbortReturnAdjust = adjust;
        abortReturnPending = false;
    };

    if (pc == lr) {
        apply_pc(lr, 0, "MOVS PC, LR");
        return;
    }
    if (pc == (lr - 4)) {
        apply_pc(lr - 4, 4, "SUBS PC, LR, #4");
        return;
    }
    if (pc == (lr - 8)) {
        apply_pc(lr - 8, 8, "SUBS PC, LR, #8");
        return;
    }
    if (lastFaultWasInstruction && pc == (lr - 2)) {
        apply_pc(lr - 2, 2, "SUBS PC, LR, #2 (thumb prefetch)");
        return;
    }

    const u32 fallback_pc = lr - lastAbortReturnAdjust;
    apply_pc(fallback_pc, lastAbortReturnAdjust, "fallback");
    return;
}
