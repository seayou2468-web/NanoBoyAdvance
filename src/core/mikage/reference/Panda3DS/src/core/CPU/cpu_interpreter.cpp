#include "cpu_interpreter.hpp"

#include "arm_defs.hpp"
#include "emulator.hpp"
#include "kernel.hpp"

CPU::CPU(Memory& mem, Kernel& kernel, Emulator& emu)
	: mem(mem), scheduler(emu.getScheduler()), kernel(kernel), emu(emu) {
	mem.setCPUTicks(getTicksRef());
}

void CPU::reset() {
	gprs.fill(0);
	extRegs.fill(0);
	cpsr = CPSR::UserMode;
	fpscr = FPSCR::MainThreadDefault;
	tlsBase = VirtualAddrs::TLSBase;
}

bool CPU::conditionPassed(u32 cond) const {
	// Ported from Cytrus/Citra DynCom CondPassed-style logic.
	const bool n = (cpsr & CPSR::Sign) != 0;
	const bool z = (cpsr & CPSR::Zero) != 0;
	const bool c = (cpsr & CPSR::Carry) != 0;
	const bool v = (cpsr & CPSR::Overflow) != 0;

	switch (cond) {
		case 0x0: return z;             // EQ
		case 0x1: return !z;            // NE
		case 0x2: return c;             // CS/HS
		case 0x3: return !c;            // CC/LO
		case 0x4: return n;             // MI
		case 0x5: return !n;            // PL
		case 0x6: return v;             // VS
		case 0x7: return !v;            // VC
		case 0x8: return c && !z;       // HI
		case 0x9: return !c || z;       // LS
		case 0xA: return n == v;        // GE
		case 0xB: return n != v;        // LT
		case 0xC: return !z && (n == v);// GT
		case 0xD: return z || (n != v); // LE
		case 0xE: return true;          // AL
		default: return false;          // NV
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

u32 CPU::executeArm(u32 inst) {
	const u32 cond = inst >> 28;
	if (!conditionPassed(cond)) {
		gprs[15] += 4;
		return 1;
	}

	if ((inst & 0x0F000000u) == 0x0F000000u) {
		const u32 svc = inst & 0x00FFFFFFu;
		gprs[15] += 4;
		kernel.serviceSVC(svc);
		return 1;
	}

	if ((inst & 0x0E000000u) == 0x0A000000u) {
		s32 imm24 = static_cast<s32>(inst & 0x00FFFFFFu);
		if (imm24 & 0x00800000) imm24 |= ~0x00FFFFFF;
		const u32 old_pc = gprs[15];
		const u32 target = old_pc + 8 + static_cast<u32>(imm24 << 2);
		if (inst & (1u << 24)) {
			gprs[14] = old_pc + 4;
		}
		gprs[15] = target;
		return 3;
	}

	if ((inst & 0x0FFFFFF0u) == 0x012FFF10u) {
		const u32 rm = inst & 0xF;
		const u32 target = gprs[rm];
		if (inst & (1u << 5)) {
			gprs[14] = gprs[15] + 4;
		}
		if (target & 1u) cpsr |= CPSR::Thumb;
		else cpsr &= ~CPSR::Thumb;
		gprs[15] = target & ~1u;
		return 3;
	}

	if ((inst & 0x0C000000u) == 0x00000000u) {
		const bool immediate = (inst & (1u << 25)) != 0;
		const u32 opcode = (inst >> 21) & 0xF;
		const bool set_flags = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		u32 op2 = 0;
		if (immediate) {
			u32 imm8 = inst & 0xFF;
			u32 rot = ((inst >> 8) & 0xF) * 2;
			op2 = (imm8 >> rot) | (imm8 << ((32 - rot) & 31));
		} else {
			op2 = gprs[inst & 0xF];
		}
		u32 lhs = gprs[rn];
		u32 result = 0;
		switch (opcode) {
			case 0x4: result = lhs + op2; if (set_flags) setAddFlags(lhs, op2, result); gprs[rd] = result; break; // ADD
			case 0x2: result = lhs - op2; if (set_flags) setSubFlags(lhs, op2, result); gprs[rd] = result; break; // SUB
			case 0xD: result = op2; if (set_flags) setNZFlags(result); gprs[rd] = result; break; // MOV
			case 0xA: result = lhs - op2; setSubFlags(lhs, op2, result); break; // CMP
			default: break;
		}
		gprs[15] += 4;
		return 1;
	}

	if ((inst & 0x0C000000u) == 0x04000000u) {
		const bool load = (inst & (1u << 20)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 offset = inst & 0xFFF;
		u32 addr = gprs[rn] + (up ? offset : static_cast<u32>(-static_cast<s32>(offset)));
		if (load) gprs[rd] = mem.read32(addr);
		else mem.write32(addr, gprs[rd]);
		gprs[15] += 4;
		return 2;
	}

	// Unsupported opcode: advance PC to keep progress.
	gprs[15] += 4;
	return 1;
}

u32 CPU::executeThumb(u16 inst) {
	// Minimum Thumb subset sufficient for scheduler-forward progress + SVC.
	if ((inst & 0xFF00u) == 0xDF00u) {
		const u32 svc = inst & 0xFFu;
		gprs[15] += 2;
		kernel.serviceSVC(svc);
		return 1;
	}

	if ((inst & 0xF800u) == 0xE000u) {
		s32 imm11 = static_cast<s32>(inst & 0x7FFu);
		if (imm11 & 0x400) imm11 |= ~0x7FF;
		gprs[15] = gprs[15] + 4 + static_cast<u32>(imm11 << 1);
		return 2;
	}

	gprs[15] += 2;
	return 1;
}

void CPU::runFrame() {
	emu.frameDone = false;

	while (!emu.frameDone) {
		u64 ticksLeft = scheduler.nextTimestamp - scheduler.currentTimestamp;
		while (!emu.frameDone && ticksLeft > 0) {
			u32 consumed = 0;
			if ((cpsr & CPSR::Thumb) != 0) {
				consumed = executeThumb(mem.read16(gprs[15]));
			} else {
				consumed = executeArm(mem.read32(gprs[15]));
			}

			scheduler.currentTimestamp += consumed;
			if (consumed >= ticksLeft) {
				ticksLeft = 0;
			} else {
				ticksLeft -= consumed;
			}
		}

		emu.pollScheduler();
	}
}
