#pragma once

#include <array>
#include <span>

#include "arm_defs.hpp"
#include "helpers.hpp"
#include "memory.hpp"
#include "scheduler.hpp"

class Emulator;
class Kernel;

// ARM11 interpreter backend (non-JIT).
// Instruction condition handling follows the Cytrus/Citra DynCom interpreter model.
class CPU {
	std::array<u32, 16> gprs{};
	std::array<u32, 64> extRegs{};
	u32 cpsr = CPSR::UserMode;
	u32 fpscr = FPSCR::MainThreadDefault;
	u32 tlsBase = 0;

	Memory& mem;
	Scheduler& scheduler;
	Kernel& kernel;
	Emulator& emu;

	bool conditionPassed(u32 cond) const;
	void setNZFlags(u32 value);
	void setAddFlags(u32 lhs, u32 rhs, u32 result);
	void setSubFlags(u32 lhs, u32 rhs, u32 result);
	u32 executeArm(u32 inst);
	u32 executeThumb(u16 inst);

  public:
	static constexpr u64 ticksPerSec = Scheduler::arm11Clock;

	CPU(Memory& mem, Kernel& kernel, Emulator& emu);
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

	u64 getTicks() { return scheduler.currentTimestamp; }
	u64& getTicksRef() { return scheduler.currentTimestamp; }
	Scheduler& getScheduler() { return scheduler; }

	void addTicks(u64 ticks) { scheduler.currentTimestamp += ticks; }

	void clearCache() {}
	void clearCacheRange(u32, u32) {}

	void runFrame();
};
