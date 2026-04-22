
#include "../../../include/cpu_interpreter.hpp"
#include "./cpu_interpreter_internal.hpp"

u32 CPU::getAddr(u32 inst, u32 old_pc) {
    const bool immediate = (inst & (1u << 25)) == 0;
    const bool pre = (inst & (1u << 24)) != 0;
    const bool up = (inst & (1u << 23)) != 0;
    const bool write_back = (inst & (1u << 21)) != 0;
    const u32 rn_idx = (inst >> 16) & 0xF;
    const u32 base = (rn_idx == 15) ? ((old_pc + 8) & ~3u) : gprs[rn_idx];

    u32 offset = 0;
    if (immediate) {
        offset = inst & 0xFFFu;
    } else {
        const u32 rm_idx = inst & 0xF;
        const u32 rm = (rm_idx == 15) ? (old_pc + 8) : gprs[rm_idx];
        const u32 shift_type = (inst >> 5) & 0x3;
        const u32 shift_imm = (inst >> 7) & 0x1F;
        switch (shift_type) {
            case 0: offset = rm << shift_imm; break;
            case 1: offset = (shift_imm == 0) ? 0 : (rm >> shift_imm); break;
            case 2: offset = (shift_imm == 0) ? (((rm >> 31) & 1) ? 0xFFFFFFFF : 0) : static_cast<u32>(static_cast<s32>(rm) >> shift_imm); break;
            case 3: offset = (shift_imm == 0) ? (((cpsr & CPSR::Carry) ? 1u : 0u) << 31) | (rm >> 1) : ror32(rm, shift_imm); break;
        }
    }

    u32 effective = pre ? (up ? (base + offset) : (base - offset)) : base;
    if (write_back || !pre) {
        gprs[rn_idx] = up ? (base + offset) : (base - offset);
    }
    return effective;
}

u32 CPU::getShifterOperand(u32 inst, bool& shifter_carry) {
	const bool immediate = (inst & (1u << 25)) != 0;
	if (immediate) {
		const u32 imm8 = inst & 0xFFu;
		const u32 rotate = ((inst >> 8) & 0xFu) * 2u;
		const u32 value = ror32(imm8, rotate);
		shifter_carry = (rotate == 0) ? ((cpsr & CPSR::Carry) != 0) : ((value >> 31) & 1u) != 0;
		return value;
	}

	const u32 rm_idx = inst & 0xFu;
	const u32 rm = (rm_idx == 15) ? (gprs[PC_INDEX] + 8) : gprs[rm_idx];
	const bool reg_shift = ((inst >> 4) & 1u) != 0;
	const u32 shift_type = (inst >> 5) & 0x3u;
	const u32 shift_amount = reg_shift ? (gprs[(inst >> 8) & 0xFu] & 0xFFu) : ((inst >> 7) & 0x1Fu);

	if (shift_amount == 0) {
		shifter_carry = (cpsr & CPSR::Carry) != 0;
		return rm;
	}

	switch (shift_type) {
		case 0: // LSL
			shifter_carry = ((rm >> (32 - shift_amount)) & 1u) != 0;
			return rm << shift_amount;
		case 1: // LSR
			shifter_carry = ((rm >> (shift_amount - 1)) & 1u) != 0;
			return (shift_amount >= 32) ? 0 : (rm >> shift_amount);
		case 2: // ASR
			shifter_carry = ((rm >> (shift_amount - 1)) & 1u) != 0;
			return static_cast<u32>(static_cast<s32>(rm) >> ((shift_amount >= 32) ? 31 : shift_amount));
		case 3: // ROR/RRX
			if (shift_amount == 0) {
				const u32 carry_in = (cpsr & CPSR::Carry) ? 1u : 0u;
				shifter_carry = (rm & 1u) != 0;
				return (carry_in << 31) | (rm >> 1);
			}
			shifter_carry = ((rm >> ((shift_amount - 1) & 31u)) & 1u) != 0;
			return ror32(rm, shift_amount & 31u);
	}
	return rm;
}

bool CPU::CurrentModeHasSPSR() const {
	const u32 mode = cpsr & 0x1Fu;
	return mode != CPSR::UserMode && mode != CPSR::SystemMode;
}

void CPU::ChangePrivilegeMode(u32 mode) {
	cpsr = (cpsr & ~0x1Fu) | (mode & 0x1Fu);
}
