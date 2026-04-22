#include "../../../include/cpu_interpreter.hpp"
#include "../../../include/emulator.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "./cpu_interpreter_internal.hpp"

CPU::CPU(Memory& mem, Kernel& kernel, Emulator& emu)
	: mem(mem), scheduler(emu.getScheduler()), kernel(kernel), emu(emu) {
	mem.setCPUTicks(getTicksRef());
}

void CPU::reset() {
	gprs.fill(0);
	extRegs.fill(0);
	cpsr = CPSR::UserMode;
	NFlag = 0; ZFlag = 1; CFlag = 0; VFlag = 0; TFlag = 0;
	tlsBase = 0x1FF80000;
	cp15SCTLR = 0x00C50078u;
	cp15ACTLR = 0;
	cp15TTBR0 = 0;
	cp15TTBR1 = 0;
	cp15TTBCR = 0;
	cp15DACR = 0;
	cp15DFSR = 0;
	cp15IFSR = 0;
	cp15DFAR = 0;
	cp15IFAR = 0;
	VfpCoreExecutor::ResetSystemRegs(fpscr, vfpFPEXC, vfpFPINST, vfpFPINST2, vfpFPSID, vfpMVFR0,
									 vfpMVFR1);
	exclusiveMonitor.Clear();
	itCond = 0;
	itMask = 0;
}

bool CPU::conditionPassed(u32 cond) const {
	const bool n = (cpsr & CPSR::Sign) != 0;
	const bool z = (cpsr & CPSR::Zero) != 0;
	const bool c = (cpsr & CPSR::Carry) != 0;
	const bool v = (cpsr & CPSR::Overflow) != 0;

	switch (cond & 0xF) {
		case 0x0: return z;
		case 0x1: return !z;
		case 0x2: return c;
		case 0x3: return !c;
		case 0x4: return n;
		case 0x5: return !n;
		case 0x6: return v;
		case 0x7: return !v;
		case 0x8: return c && !z;
		case 0x9: return !c || z;
		case 0xA: return n == v;
		case 0xB: return n != v;
		case 0xC: return !z && (n == v);
		case 0xD: return z || (n != v);
		case 0xE:
		case 0xF: return true;
		default: return false;
	}
}

void CPU::setNZFlags(u32 value) {
	cpsr = (cpsr & ~(CPSR::Sign | CPSR::Zero)) |
		       ((value & 0x80000000u) ? CPSR::Sign : 0) |
		       ((value == 0) ? CPSR::Zero : 0);
}

void CPU::setAddFlags(u32 lhs, u32 rhs, u32 result) {
	setNZFlags(result);
	const u64 wide = static_cast<u64>(lhs) + static_cast<u64>(rhs);
	const bool carry = (wide >> 32) != 0;
	const bool overflow = ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0;
	cpsr = (cpsr & ~(CPSR::Carry | CPSR::Overflow)) |
		       (carry ? CPSR::Carry : 0) |
		       (overflow ? CPSR::Overflow : 0);
}

void CPU::setSubFlags(u32 lhs, u32 rhs, u32 result) {
	setNZFlags(result);
	const bool carry = lhs >= rhs;
	const bool overflow = (((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0;
	cpsr = (cpsr & ~(CPSR::Carry | CPSR::Overflow)) |
		       (carry ? CPSR::Carry : 0) |
		       (overflow ? CPSR::Overflow : 0);
}

u32 CPU::getRegOperand(u32 index, u32 old_pc) {
	if (index == PC_INDEX) return old_pc + 8;
	return gprs[index];
}

void CPU::setCarry(bool value) {
	if (value) cpsr |= CPSR::Carry;
	else cpsr &= ~CPSR::Carry;
}

void CPU::writeReg(u32 index, u32 value) {
	if (index == PC_INDEX) {
		gprs[PC_INDEX] = value & ~1u;
		if (value & 1u) cpsr |= CPSR::Thumb;
	} else {
		gprs[index] = value;
	}
}

void CPU::clearExclusive() {
	exclusiveMonitor.Clear();
}

void CPU::runFrame() {
	emu.frameDone = false;
	while (!emu.frameDone) {
		u64 ticksLeft = scheduler.nextTimestamp - scheduler.currentTimestamp;
		while (!emu.frameDone && ticksLeft > 0) {
			u32 consumed = 0;
			if ((cpsr & CPSR::Thumb) != 0) {
				consumed = executeThumb(mem.read16(gprs[PC_INDEX]));
			} else {
				consumed = executeArm(mem.read32(gprs[PC_INDEX]));
			}
			scheduler.currentTimestamp += consumed;
			if (consumed >= ticksLeft) ticksLeft = 0;
			else ticksLeft -= consumed;
		}
		emu.pollScheduler();
	}
}

void CPU::loadNZCVT() {
	NFlag = (cpsr >> 31);
	ZFlag = (cpsr >> 30) & 1;
	CFlag = (cpsr >> 29) & 1;
	VFlag = (cpsr >> 28) & 1;
	TFlag = (cpsr >> 5) & 1;
}

void CPU::saveNZCVT() {
	cpsr = (cpsr & 0x0FFFFFDFu) | (NFlag << 31) | (ZFlag << 30) | (CFlag << 29) | (VFlag << 28) | (TFlag << 5);
}
