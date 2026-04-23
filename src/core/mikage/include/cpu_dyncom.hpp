#pragma once

#include <array>
#include <memory>
#include <span>

#include "./arm_defs.hpp"
#include "./helpers.hpp"
#include "./memory.hpp"
#include "./scheduler.hpp"

class Emulator;
class Kernel;
struct ARMul_State;

class ExclusiveMonitor {
public:
	void Set(u32 addr, u32 size = 4) {
		address = addr;
		size_mask = ~(size - 1);
		valid = true;
	}
	bool TryWrite(u32 addr) {
		if (!valid || (addr & size_mask) != (address & size_mask)) return false;
		valid = false;
		return true;
	}
	void Clear() { valid = false; }

private:
	u32 address = 0;
	u32 size_mask = 0;
	bool valid = false;
};

// ARM11 DynCom backend wrapper.
// Core execution uses src/core/mikage/src/core/CPU/dyncom/*.
// This front-end keeps Panda3DS CPU-facing API stable.
class CPU {
	std::array<u32, 16> gprs{};
	std::array<u32, 64> extRegs{};
	u32 cpsr = CPSR::UserMode;
	u32 fpscr = FPSCR::MainThreadDefault;
	u32 tlsBase = 0;
	u32 cp15SCTLR = 0;
	u32 cp15ACTLR = 0;
	u32 cp15TTBR0 = 0;
	u32 cp15TTBR1 = 0;
	u32 cp15TTBCR = 0;
	u32 cp15DACR = 0;
	u32 cp15DFSR = 0;
	u32 cp15IFSR = 0;
	u32 cp15DFAR = 0;
	u32 cp15IFAR = 0;
	u32 vfpFPEXC = 0;
	u32 vfpFPINST = 0;
	u32 vfpFPINST2 = 0;
	u32 vfpFPSID = 0;
	u32 lastAbortReturnAdjust = 0;
	bool lastFaultWasInstruction = false;
	bool abortReturnPending = false;
	u32 vfpMVFR0 = 0;
	u32 vfpMVFR1 = 0;
	u32 exclusiveAddress = 0;
	u32 exclusiveSize = 0;
	bool exclusiveValid = false;
	u8 itCond = 0;
	u8 itMask = 0;
	u32 NFlag = 0;
	u32 ZFlag = 1;
	u32 CFlag = 0;
	u32 VFlag = 0;
	u32 TFlag = 0;


	std::unique_ptr<ARMul_State> dyncomState;
	ExclusiveMonitor exclusiveMonitor;

	Memory& mem;
	Scheduler* scheduler = nullptr;
	Kernel* kernel = nullptr;
	Emulator& emu;

	bool conditionPassed(u32 cond) const;
	void setNZFlags(u32 value);
	void setAddFlags(u32 lhs, u32 rhs, u32 result);
	void setSubFlags(u32 lhs, u32 rhs, u32 result);
	u32 getRegOperand(u32 index, u32 old_pc);
	void setCarry(bool value);
	void writeReg(u32 index, u32 value);
	u32 getShifterOperand(u32 inst, bool& shifter_carry);
	bool CurrentModeHasSPSR() const;
	void ChangePrivilegeMode(u32 mode);
	u32 executeArm(u32 inst);
	u32 executeThumb(u16 inst);
	u32 executeCoproc(u32 inst);
	u32 executeVfp(u32 inst);
	u32 executeNeon(u32 inst);
	u32 getAddr(u32 inst, u32 old_pc);

  public:
	static constexpr u64 ticksPerSec = Scheduler::arm11Clock;

	CPU(Memory& mem, Emulator& emu, Scheduler& schedulerRef);
	~CPU();
	void bindScheduler(Scheduler& schedulerRef) { scheduler = &schedulerRef; }
	void bindKernel(Kernel& kernelRef) { kernel = &kernelRef; }
	void reset();

		void setReg(int index, u32 value) { gprs[static_cast<size_t>(index)] = value; }
	u32 getReg(int index) { return gprs[static_cast<size_t>(index)]; }

	std::span<u32, 16> regs() { return gprs; }
	std::span<u32, 32> fprs() { return std::span(extRegs).first<32>(); }

	void setCPSR(u32 value) { cpsr = value; }
	u32 getCPSR() { return cpsr; }

	void setFPSCR(u32 value) { fpscr = value; }
	u32 getFPSCR() { return fpscr; }

	void setTLSBase(u32 value) { tlsBase = value; }

	u64 getTicks();
	u64& getTicksRef();
	Scheduler& getScheduler();

	void addTicks(u64 ticks);
	void reportMMUFault(u32 fsr, u32 far, bool instruction_fault);
	u32 getLastAbortReturnAdjust() const { return lastAbortReturnAdjust; }
	void finalizeAbortReturnIfNeeded();

	void clearCache() {}
	void clearCacheRange(u32, u32) {}
	void clearExclusive();

	void runFrame();

	void loadNZCVT();
	void saveNZCVT();
};
