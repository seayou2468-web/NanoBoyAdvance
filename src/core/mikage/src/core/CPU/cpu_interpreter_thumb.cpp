#include "../../../include/cpu_interpreter.hpp"
#include "../../../include/emulator.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "./cpu_interpreter_internal.hpp"
#include "../../../include/logger.hpp"

u32 CPU::executeThumb(u16 inst) {
	const u32 old_pc = gprs[PC_INDEX];
	const bool is_thumb32_prefix = ((inst & 0xE000u) == 0xE000u) && ((inst & 0x1800u) != 0);
	const bool is_it = (inst & 0xFF00u) == 0xBF00u && (inst & 0xF) != 0 && ((inst >> 4) & 0xF) != 0xF;
	struct ITAdvanceGuard {
		u8& mask;
		u8& cond;
		bool enabled;
		~ITAdvanceGuard() {
			if (!enabled || mask == 0) {
				return;
			}
			mask = static_cast<u8>((mask << 1) & 0xF);
			if (mask == 0) {
				cond = 0;
			}
		}
	} it_guard{itMask, itCond, itMask != 0 && !is_it};
	const auto set_carry = [&](bool value) {
		if (value) cpsr |= CPSR::Carry;
		else cpsr &= ~CPSR::Carry;
	};
	if (itMask != 0 && !is_it) {
		const bool then = (itMask & 0x8u) != 0;
		const u32 cond = then ? itCond : (itCond ^ 1u);
		if (!conditionPassed(cond)) {
			gprs[PC_INDEX] = old_pc + (is_thumb32_prefix ? 4u : 2u);
			it_guard.enabled = false;
			if (itMask != 0) {
				itMask = static_cast<u8>((itMask << 1) & 0xF);
				if (itMask == 0) {
					itCond = 0;
				}
			}
			return 1;
		}
	}
	if ((inst & 0xF800u) == 0x1800u) {
		const bool immediate = (inst & (1u << 10)) != 0;
		const bool sub = (inst & (1u << 9)) != 0;
		const u32 rn = (inst >> 3) & 0x7;
		const u32 rd = inst & 0x7;
		const u32 rhs = immediate ? ((inst >> 6) & 0x7) : gprs[(inst >> 6) & 0x7];
		const u32 lhs = gprs[rn];
		u32 result = sub ? (lhs - rhs) : (lhs + rhs);
		gprs[rd] = result;
		if (sub) setSubFlags(lhs, rhs, result);
		else setAddFlags(lhs, rhs, result);
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	// BKPT
	if ((inst & 0xFF00u) == 0xBE00u) {
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	// IT
	if (is_it) {
		it_guard.enabled = false;
		itCond = static_cast<u8>((inst >> 4) & 0xF);
		itMask = static_cast<u8>(inst & 0xF);
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	// Hints (NOP/YIELD/WFE/WFI/SEV)
	if ((inst & 0xFF0Fu) == 0xBF00u) {
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xE000u) == 0x0000u) {
		const u32 opcode = (inst >> 11) & 0x3;
		const u32 offset = (inst >> 6) & 0x1F;
		const u32 rs = (inst >> 3) & 0x7;
		const u32 rd = inst & 0x7;
		u32 value = gprs[rs];
		u32 result = value;
		bool carry = (cpsr & CPSR::Carry) != 0;
		switch (opcode) {
			case 0:  // LSL
				if (offset != 0) {
					carry = ((value >> (32 - offset)) & 1u) != 0;
					result = value << offset;
				}
				break;
			case 1:  // LSR
				if (offset == 0) {
					carry = (value >> 31) != 0;
					result = 0;
				} else {
					carry = ((value >> (offset - 1)) & 1u) != 0;
					result = value >> offset;
				}
				break;
			case 2:  // ASR
				if (offset == 0) {
					carry = (value >> 31) != 0;
					result = (value & 0x80000000u) ? 0xFFFFFFFFu : 0;
				} else {
					carry = ((value >> (offset - 1)) & 1u) != 0;
					result = static_cast<u32>(static_cast<s32>(value) >> offset);
				}
				break;
			default: break;
		}
		gprs[rd] = result;
		setNZFlags(result);
		set_carry(carry);
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xE000u) == 0x2000u) {
		const u32 op = (inst >> 11) & 0x3;
		const u32 rd = (inst >> 8) & 0x7;
		const u32 imm8 = inst & 0xFF;
		const u32 lhs = gprs[rd];
		u32 result = 0;
		switch (op) {
			case 0: result = imm8; gprs[rd] = result; setNZFlags(result); break;             // MOV
			case 1: result = lhs - imm8; setSubFlags(lhs, imm8, result); break;              // CMP
			case 2: result = lhs + imm8; gprs[rd] = result; setAddFlags(lhs, imm8, result); break; // ADD
			case 3: result = lhs - imm8; gprs[rd] = result; setSubFlags(lhs, imm8, result); break; // SUB
		}
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xFC00u) == 0x4000u) {
		const u32 opcode = (inst >> 6) & 0xF;
		const u32 rs = (inst >> 3) & 0x7;
		const u32 rd = inst & 0x7;
		const u32 lhs = gprs[rd];
		const u32 rhs = gprs[rs];
		u32 result = lhs;
		switch (opcode) {
			case 0x0: result = lhs & rhs; gprs[rd] = result; setNZFlags(result); break;
			case 0x1: result = lhs ^ rhs; gprs[rd] = result; setNZFlags(result); break;
			case 0x2: {
				const u32 shift = rhs & 0xFF;
				if (shift == 0) result = lhs;
				else if (shift < 32) {
					set_carry(((lhs >> (32 - shift)) & 1u) != 0);
					result = lhs << shift;
				} else if (shift == 32) {
					set_carry((lhs & 1u) != 0);
					result = 0;
				} else {
					set_carry(false);
					result = 0;
				}
				gprs[rd] = result;
				setNZFlags(result);
				break;
			}
			case 0x3: {
				const u32 shift = rhs & 0xFF;
				if (shift == 0) result = lhs;
				else if (shift < 32) {
					set_carry(((lhs >> (shift - 1)) & 1u) != 0);
					result = lhs >> shift;
				} else if (shift == 32) {
					set_carry((lhs >> 31) != 0);
					result = 0;
				} else {
					set_carry(false);
					result = 0;
				}
				gprs[rd] = result;
				setNZFlags(result);
				break;
			}
			case 0x4: {
				const u32 shift = rhs & 0xFF;
				if (shift == 0) result = lhs;
				else if (shift < 32) {
					set_carry(((lhs >> (shift - 1)) & 1u) != 0);
					result = static_cast<u32>(static_cast<s32>(lhs) >> shift);
				} else {
					set_carry((lhs >> 31) != 0);
					result = (lhs & 0x80000000u) ? 0xFFFFFFFFu : 0;
				}
				gprs[rd] = result;
				setNZFlags(result);
				break;
			}
			case 0x5: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs + rhs + carry;
				gprs[rd] = result;
				setAddFlags(lhs, rhs + carry, result);
				break;
			}
			case 0x6: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs - rhs - (1u - carry);
				gprs[rd] = result;
				setSubFlags(lhs, rhs + (1u - carry), result);
				break;
			}
			case 0x7: {
				const u32 shift = rhs & 31;
				if (shift == 0) {
					result = lhs;
				} else {
					result = ror32(lhs, shift);
					set_carry((result >> 31) != 0);
				}
				gprs[rd] = result;
				setNZFlags(result);
				break;
			}
			case 0x8: result = lhs & rhs; setNZFlags(result); break;  // TST
			case 0x9: result = 0 - rhs; gprs[rd] = result; setSubFlags(0, rhs, result); break; // NEG
			case 0xA: result = lhs - rhs; setSubFlags(lhs, rhs, result); break;  // CMP
			case 0xB: result = lhs + rhs; setAddFlags(lhs, rhs, result); break;  // CMN
			case 0xC: result = lhs | rhs; gprs[rd] = result; setNZFlags(result); break;
			case 0xD: result = lhs * rhs; gprs[rd] = result; setNZFlags(result); break;
			case 0xE: result = lhs & ~rhs; gprs[rd] = result; setNZFlags(result); break;
			case 0xF: result = ~rhs; gprs[rd] = result; setNZFlags(result); break;
		}
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xFC00u) == 0x4400u) {
		const u32 op = (inst >> 8) & 0x3;
		const u32 h1 = (inst >> 7) & 0x1;
		const u32 h2 = (inst >> 6) & 0x1;
		const u32 rs = ((h2 << 3) | ((inst >> 3) & 0x7));
		const u32 rd = ((h1 << 3) | (inst & 0x7));
		const u32 lhs = gprs[rd];
		const u32 rhs = gprs[rs];
		switch (op) {
			case 0: {
				const u32 result = lhs + rhs;
				if (rd == PC_INDEX) {
					gprs[PC_INDEX] = result & ~1u;
					if (result & 1u) cpsr |= CPSR::Thumb;
				} else {
					gprs[rd] = result;
					gprs[PC_INDEX] = old_pc + 2;
				}
				return 1;
			}
			case 1: {
				const u32 result = lhs - rhs;
				setSubFlags(lhs, rhs, result);
				gprs[PC_INDEX] = old_pc + 2;
				return 1;
			}
			case 2:
				if (rd == PC_INDEX) {
					gprs[PC_INDEX] = rhs & ~1u;
					if (rhs & 1u) cpsr |= CPSR::Thumb;
					else cpsr &= ~CPSR::Thumb;
				} else {
					gprs[rd] = rhs;
					gprs[PC_INDEX] = old_pc + 2;
				}
				return 1;
			case 3:
				gprs[LR_INDEX] = (old_pc + 2) | 1u;
				gprs[PC_INDEX] = rhs & ~1u;
				if (rhs & 1u) cpsr |= CPSR::Thumb;
				else cpsr &= ~CPSR::Thumb;
				return 3;
		}
	}

	if ((inst & 0xF800u) == 0x4800u) {
		const u32 rd = (inst >> 8) & 0x7;
		const u32 imm = (inst & 0xFF) << 2;
		const u32 addr = (old_pc + 4) & ~3u;
		gprs[rd] = mem.read32(addr + imm);
		gprs[PC_INDEX] = old_pc + 2;
		return 2;
	}

	if ((inst & 0xF000u) == 0x5000u || (inst & 0xE000u) == 0x6000u || (inst & 0xF000u) == 0x8000u ||
		(inst & 0xF000u) == 0x9000u) {
		u32 addr = 0;
		u32 rd = 0;
		bool load = false;
		bool byte = false;
		bool half = false;
		bool sign_extend_byte = false;
		bool sign_extend_half = false;

		if ((inst & 0xF000u) == 0x5000u) {
			const u32 ro = (inst >> 6) & 0x7;
			const u32 rb = (inst >> 3) & 0x7;
			const bool sign_half = (inst & (1u << 9)) != 0;
			rd = inst & 0x7;
			addr = gprs[rb] + gprs[ro];
			if (!sign_half) {
				load = (inst & (1u << 11)) != 0;
				byte = (inst & (1u << 10)) != 0;
			} else {
				const bool h = (inst & (1u << 11)) != 0;
				const bool sbit = (inst & (1u << 10)) != 0;
				if (!h && !sbit) {       // STRH
					load = false;
					half = true;
				} else if (!h && sbit) { // LDRSB
					load = true;
					sign_extend_byte = true;
				} else if (h && !sbit) { // LDRH
					load = true;
					half = true;
				} else {                 // LDRSH
					load = true;
					sign_extend_half = true;
				}
			}
		} else if ((inst & 0xE000u) == 0x6000u) {
			load = (inst & (1u << 11)) != 0;
			byte = (inst & (1u << 12)) != 0;
			const u32 imm5 = (inst >> 6) & 0x1F;
			const u32 rb = (inst >> 3) & 0x7;
			rd = inst & 0x7;
			addr = gprs[rb] + (byte ? imm5 : (imm5 << 2));
		} else if ((inst & 0xF000u) == 0x8000u) {
			load = (inst & (1u << 11)) != 0;
			half = true;
			const u32 imm5 = (inst >> 6) & 0x1F;
			const u32 rb = (inst >> 3) & 0x7;
			rd = inst & 0x7;
			addr = gprs[rb] + (imm5 << 1);
		} else {
			load = (inst & (1u << 11)) != 0;
			const u32 rdv = (inst >> 8) & 0x7;
			const u32 imm8 = inst & 0xFF;
			rd = rdv;
			addr = gprs[13] + (imm8 << 2);
		}

		if (load) {
			if (sign_extend_byte) gprs[rd] = static_cast<u32>(static_cast<s32>(static_cast<s8>(mem.read8(addr))));
			else if (sign_extend_half) gprs[rd] = static_cast<u32>(static_cast<s32>(static_cast<s16>(mem.read16(addr))));
			else if (byte) gprs[rd] = mem.read8(addr);
			else if (half) gprs[rd] = mem.read16(addr);
			else gprs[rd] = mem.read32(addr);
		} else {
			if (byte) mem.write8(addr, static_cast<u8>(gprs[rd]));
			else if (half) mem.write16(addr, static_cast<u16>(gprs[rd]));
			else mem.write32(addr, gprs[rd]);
			exclusiveMonitor.Clear();
		}
		gprs[PC_INDEX] = old_pc + 2;
		return 2;
	}

	if ((inst & 0xF000u) == 0xA000u) {
		const bool sp = (inst & (1u << 11)) != 0;
		const u32 rd = (inst >> 8) & 0x7;
		const u32 imm = (inst & 0xFF) << 2;
		gprs[rd] = (sp ? gprs[13] : ((old_pc + 4) & ~3u)) + imm;
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xFF00u) == 0xB000u) {
		const u32 imm = (inst & 0x7F) << 2;
		if (inst & (1u << 7)) gprs[13] -= imm;
		else gprs[13] += imm;
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	// CBZ/CBNZ
	if ((inst & 0xF500u) == 0xB100u) {
		const bool nonzero = (inst & (1u << 11)) != 0;
		const u32 i = (inst >> 9) & 0x1;
		const u32 imm5 = (inst >> 3) & 0x1F;
		const u32 rn = inst & 0x7;
		const u32 offset = (i << 6) | (imm5 << 1);
		const bool is_zero = (gprs[rn] == 0);
		if ((is_zero && !nonzero) || (!is_zero && nonzero)) {
			gprs[PC_INDEX] = old_pc + 4 + offset;
		} else {
			gprs[PC_INDEX] = old_pc + 2;
		}
		return 1;
	}

	// REV / REV16 / REVSH
	if ((inst & 0xFFC0u) == 0xBA00u) {
		const u32 opcode = (inst >> 6) & 0x3;
		const u32 rm = (inst >> 3) & 0x7;
		const u32 rd = inst & 0x7;
		const u32 value = gprs[rm];
		u32 result = value;
		switch (opcode) {
			case 0x0:
				result = ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
						 ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
				break;
			case 0x1:
				result = ((value & 0x00FF00FFu) << 8) | ((value & 0xFF00FF00u) >> 8);
				break;
			case 0x3: {
				const u16 half = static_cast<u16>(((value & 0x00FFu) << 8) | ((value & 0xFF00u) >> 8));
				result = static_cast<u32>(static_cast<s32>(static_cast<s16>(half)));
				break;
			}
			default:
				gprs[PC_INDEX] = old_pc + 2;
				return 1;
		}
		gprs[rd] = result;
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	// SXTH / SXTB / UXTH / UXTB
	if ((inst & 0xFF00u) == 0xB200u) {
		const u32 op = (inst >> 6) & 0x3;
		const u32 rm = (inst >> 3) & 0x7;
		const u32 rd = inst & 0x7;
		const u32 value = gprs[rm];
		u32 result = value;
		switch (op) {
			case 0x0: result = static_cast<u32>(static_cast<s32>(static_cast<s16>(value & 0xFFFFu))); break; // SXTH
			case 0x1: result = static_cast<u32>(static_cast<s32>(static_cast<s8>(value & 0xFFu))); break;   // SXTB
			case 0x2: result = value & 0xFFFFu; break; // UXTH
			case 0x3: result = value & 0xFFu; break;   // UXTB
		}
		gprs[rd] = result;
		gprs[PC_INDEX] = old_pc + 2;
		return 1;
	}

	if ((inst & 0xFE00u) == 0xB400u) {
		const bool load = (inst & (1u << 11)) != 0;
		const bool pc_lr = (inst & (1u << 8)) != 0;
		u32 reg_list = inst & 0xFF;
		if (pc_lr) {
			reg_list |= load ? (1u << PC_INDEX) : (1u << LR_INDEX);
		}
		if (load) {
			for (u32 i = 0; i < 16; i++) {
				if (reg_list & (1u << i)) {
					gprs[i] = mem.read32(gprs[13]);
					gprs[13] += 4;
				}
			}
		} else {
			for (s32 i = 15; i >= 0; i--) {
				if (reg_list & (1u << i)) {
					gprs[13] -= 4;
					mem.write32(gprs[13], gprs[static_cast<size_t>(i)]);
					exclusiveMonitor.Clear();
				}
			}
		}
		if (!load || (reg_list & (1u << PC_INDEX)) == 0) {
			gprs[PC_INDEX] = old_pc + 2;
		}
		return 2;
	}

	if ((inst & 0xF000u) == 0xC000u) {
		const bool load = (inst & (1u << 11)) != 0;
		const u32 rb = (inst >> 8) & 0x7;
		const u32 reg_list = inst & 0xFF;
		u32 addr = gprs[rb];
		for (u32 i = 0; i < 8; i++) {
			if (reg_list & (1u << i)) {
				if (load) gprs[i] = mem.read32(addr);
				else {
					mem.write32(addr, gprs[i]);
					exclusiveMonitor.Clear();
				}
				addr += 4;
			}
		}
		gprs[rb] = addr;
		gprs[PC_INDEX] = old_pc + 2;
		return 2;
	}

	if ((inst & 0xFF00u) == 0xDF00u) {
		const u32 svc = inst & 0xFF;
		gprs[PC_INDEX] = old_pc + 2;
		kernel.serviceSVC(svc);
		return 4;
	}

	if ((inst & 0xF000u) == 0xD000u && (inst & 0x0F00u) != 0x0F00u) {
		const u32 cond = (inst >> 8) & 0xF;
		const s32 offset = static_cast<s32>(signExtend(inst & 0xFF, 8) << 1);
		if (conditionPassed(cond)) {
			gprs[PC_INDEX] = old_pc + 4 + offset;
		} else {
			gprs[PC_INDEX] = old_pc + 2;
		}
		return 1;
	}

	if ((inst & 0xF800u) == 0xE000u) {
		const s32 offset = static_cast<s32>(signExtend(inst & 0x7FF, 11) << 1);
		gprs[PC_INDEX] = old_pc + 4 + offset;
		return 2;
	}

	if ((inst & 0xF800u) == 0xF000u || (inst & 0xF800u) == 0xF800u) {
		const u16 next = mem.read16(old_pc + 2);
		// Thumb-2 MOVW/MOVT (immediate)
		if ((inst & 0xFBF0u) == 0xF240u || (inst & 0xFBF0u) == 0xF2C0u) {
			const bool top = (inst & 0x0080u) != 0;
			const u32 rd = (next >> 8) & 0xF;
			const u32 imm4 = inst & 0xF;
			const u32 i = (inst >> 10) & 0x1;
			const u32 imm3 = (next >> 12) & 0x7;
			const u32 imm8 = next & 0xFF;
			const u32 imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
			if (top) {
				gprs[rd] = (gprs[rd] & 0x0000FFFFu) | (imm16 << 16);
			} else {
				gprs[rd] = imm16;
			}
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		// Thumb-2 SDIV/UDIV
		if (((inst & 0xFFF0u) == 0xFB90u || (inst & 0xFFF0u) == 0xFBB0u) && (next & 0xF0F0u) == 0xF0F0u) {
			const bool is_unsigned = (inst & 0x0020u) != 0;
			const u32 rn = inst & 0xF;
			const u32 rd = (next >> 8) & 0xF;
			const u32 rm = next & 0xF;
			u32 result = 0;
			if (gprs[rm] != 0) {
				if (is_unsigned) {
					result = gprs[rn] / gprs[rm];
				} else {
					result = static_cast<u32>(static_cast<s32>(gprs[rn]) / static_cast<s32>(gprs[rm]));
				}
			}
			gprs[rd] = result;
			gprs[PC_INDEX] = old_pc + 4;
			return 4;
		}

		// Thumb-2 data-processing (shifted register): AND/BIC/ORR/ORN/EOR/ADD/ADC/SBC/SUB/RSB and test variants.
		if ((inst & 0xFE00u) == 0xEA00u || (inst & 0xFE00u) == 0xEB00u) {
			const u32 op = (inst >> 5) & 0xF;
			const u32 rn = inst & 0xF;
			const u32 rd = (next >> 8) & 0xF;
			const u32 imm3 = (next >> 12) & 0x7;
			const u32 imm2 = (next >> 6) & 0x3;
			const u32 shift_type = (next >> 4) & 0x3;
			const u32 rm = next & 0xF;
			const u32 shift = (imm3 << 2) | imm2;

			const auto apply_shift = [&](u32 value, bool& carry_out) {
				carry_out = (cpsr & CPSR::Carry) != 0;
				switch (shift_type) {
					case 0:  // LSL
						if (shift == 0) return value;
						if (shift < 32) {
							carry_out = ((value >> (32 - shift)) & 1u) != 0;
							return value << shift;
						}
						if (shift == 32) {
							carry_out = (value & 1u) != 0;
							return 0u;
						}
						carry_out = false;
						return 0u;
					case 1:  // LSR
						if (shift == 0 || shift == 32) {
							carry_out = (value >> 31) != 0;
							return 0u;
						}
						if (shift < 32) {
							carry_out = ((value >> (shift - 1)) & 1u) != 0;
							return value >> shift;
						}
						carry_out = false;
						return 0u;
					case 2:  // ASR
						if (shift == 0 || shift >= 32) {
							carry_out = (value >> 31) != 0;
							return (value & 0x80000000u) ? 0xFFFFFFFFu : 0u;
						}
						carry_out = ((value >> (shift - 1)) & 1u) != 0;
						return static_cast<u32>(static_cast<s32>(value) >> shift);
					case 3:  // ROR
					default:
						if (shift == 0) {
							return value;
						}
						carry_out = ((value >> ((shift - 1) & 31)) & 1u) != 0;
						return ror32(value, shift);
				}
			};

			const u32 lhs = gprs[rn];
			bool shifter_carry = false;
			const u32 rhs = apply_shift(gprs[rm], shifter_carry);
			u32 result = 0;
			bool write_result = true;
			switch (op) {
				case 0x0: result = lhs & rhs; break;  // AND
				case 0x1: result = lhs & ~rhs; break; // BIC
				case 0x2: result = lhs | rhs; break;  // ORR
				case 0x3: result = lhs | ~rhs; break; // ORN
				case 0x4: result = lhs ^ rhs; break;  // EOR
				case 0x8: result = lhs + rhs; break;  // ADD
				case 0xA: {
					const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
					result = lhs + rhs + carry;
					break;
				}
				case 0xB: {
					const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
					result = lhs - rhs - (1u - carry);
					break;
				}
				case 0xD: result = lhs - rhs; break;  // SUB
				case 0xE: result = rhs - lhs; break;  // RSB
				default: write_result = false; break;
			}

			if (rd == 0xF) {
				write_result = false;
				switch (op) {
					case 0x0: setNZFlags(lhs & rhs); if (shifter_carry) cpsr |= CPSR::Carry; else cpsr &= ~CPSR::Carry; break;  // TST
					case 0x4: setNZFlags(lhs ^ rhs); if (shifter_carry) cpsr |= CPSR::Carry; else cpsr &= ~CPSR::Carry; break;  // TEQ
					case 0x8: setAddFlags(lhs, rhs, lhs + rhs); break; // CMN
					case 0xD: setSubFlags(lhs, rhs, lhs - rhs); break; // CMP
					default: break;
				}
			} else if (write_result) {
				switch (op) {
					case 0x0:
					case 0x1:
					case 0x2:
					case 0x3:
					case 0x4:
						setNZFlags(result);
						if (shifter_carry) cpsr |= CPSR::Carry;
						else cpsr &= ~CPSR::Carry;
						break;
					case 0x8: setAddFlags(lhs, rhs, result); break;
					case 0xA: {
						const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
						const u64 wide = static_cast<u64>(lhs) + static_cast<u64>(rhs) + carry;
						setNZFlags(result);
						if ((wide >> 32) != 0) cpsr |= CPSR::Carry;
						else cpsr &= ~CPSR::Carry;
						break;
					}
					case 0xB:
					case 0xD: setSubFlags(lhs, rhs, result); break;
					case 0xE: setSubFlags(rhs, lhs, result); break;
					default: break;
				}
				gprs[rd] = result;
			}

			gprs[PC_INDEX] = old_pc + 4;
			return 2;
		}

		// Thumb-2 B.W (unconditional)
		if ((inst & 0xF800u) == 0xF000u && (next & 0xD000u) == 0x9000u) {
			const u32 s = (inst >> 10) & 1u;
			const u32 imm10 = inst & 0x03FFu;
			const u32 j1 = (next >> 13) & 1u;
			const u32 j2 = (next >> 11) & 1u;
			const u32 imm11 = next & 0x07FFu;
			const u32 i1 = (~(j1 ^ s)) & 1u;
			const u32 i2 = (~(j2 ^ s)) & 1u;
			const u32 encoded = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
			const s32 offset = static_cast<s32>(signExtend(encoded, 25));
			gprs[PC_INDEX] = old_pc + 4 + offset;
			return 2;
		}

		// Thumb-2 B<cond>.W
		if ((inst & 0xF800u) == 0xF000u && (next & 0xD000u) == 0x8000u) {
			const u32 cond = (inst >> 6) & 0xF;
			if (cond != 0xE && cond != 0xF) {
				const u32 s = (inst >> 10) & 1u;
				const u32 imm6 = inst & 0x3Fu;
				const u32 j1 = (next >> 13) & 1u;
				const u32 j2 = (next >> 11) & 1u;
				const u32 imm11 = next & 0x07FFu;
				const u32 i1 = (~(j1 ^ s)) & 1u;
				const u32 i2 = (~(j2 ^ s)) & 1u;
				const u32 encoded = (s << 20) | (i1 << 19) | (i2 << 18) | (imm6 << 12) | (imm11 << 1);
				const s32 offset = static_cast<s32>(signExtend(encoded, 21));
				if (conditionPassed(cond)) {
					gprs[PC_INDEX] = old_pc + 4 + offset;
				} else {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 2;
			}
		}

		// Thumb-2 TBB/TBH
		if ((inst & 0xFFF0u) == 0xE8D0u && ((next & 0xFFE0u) == 0xF000u || (next & 0xFFE0u) == 0xF010u)) {
			const bool halfword = (next & 0x0010u) != 0;
			const u32 rn = inst & 0xF;
			const u32 rm = next & 0xF;
			const u32 base = gprs[rn];
			const u32 index = gprs[rm];
			const u32 offset = halfword ? static_cast<u32>(mem.read16(base + index * 2)) : static_cast<u32>(mem.read8(base + index));
			gprs[PC_INDEX] = old_pc + 4 + (offset << 1);
			return 2;
		}

		// Thumb-2 BLX (immediate)
		if ((inst & 0xF800u) == 0xF000u && (next & 0xD001u) == 0xC000u) {
			const u32 s = (inst >> 10) & 1u;
			const u32 imm10 = inst & 0x03FFu;
			const u32 j1 = (next >> 13) & 1u;
			const u32 j2 = (next >> 11) & 1u;
			const u32 imm10l = (next >> 1) & 0x03FFu;
			const u32 i1 = (~(j1 ^ s)) & 1u;
			const u32 i2 = (~(j2 ^ s)) & 1u;
			const u32 encoded = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm10l << 2);
			const s32 offset = static_cast<s32>(signExtend(encoded, 25));
			gprs[LR_INDEX] = (old_pc + 4) | 1u;
			gprs[PC_INDEX] = (old_pc + 4 + offset) & ~3u;
			cpsr &= ~CPSR::Thumb;
			return 3;
		}

		if ((inst & 0xF800u) == 0xF000u && (next & 0xF800u) == 0xF800u) {
			u32 hi = inst & 0x7FF;
			u32 lo = next & 0x7FF;
			s32 offset = static_cast<s32>((hi << 12) | (lo << 1));
			offset = static_cast<s32>(signExtend(offset, 23));
			gprs[LR_INDEX] = (old_pc + 4) | 1u;
			gprs[PC_INDEX] = old_pc + 4 + offset;
			return 3;
		}
	}

	gprs[PC_INDEX] = old_pc + (is_thumb32_prefix ? 4u : 2u);
	return 1;
}
