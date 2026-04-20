#include "cpu_interpreter.hpp"

#include <bit>

#include "arm_defs.hpp"
#include "emulator.hpp"
#include "kernel.hpp"

namespace {
constexpr u32 PC_INDEX = 15;
constexpr u32 LR_INDEX = 14;

[[nodiscard]] constexpr u32 ror32(u32 value, u32 shift) {
	shift &= 31;
	if (shift == 0) {
		return value;
	}
	return (value >> shift) | (value << ((32 - shift) & 31));
}

[[nodiscard]] constexpr u32 signExtend(u32 value, u32 bits) {
	const u32 shift = 32 - bits;
	return static_cast<u32>(static_cast<s32>(value << shift) >> shift);
}
}  // namespace

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
	exclusiveAddress = 0;
	exclusiveSize = 0;
	exclusiveValid = false;
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

u32 CPU::executeArm(u32 inst) {
	const u32 old_pc = gprs[PC_INDEX];
	const u32 cond = inst >> 28;
	if (!conditionPassed(cond)) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	const auto get_reg_operand = [&](u32 index) {
		if (index == PC_INDEX) {
			return old_pc + 8;
		}
		return gprs[index];
	};

	const auto set_carry = [&](bool value) {
		if (value) {
			cpsr |= CPSR::Carry;
		} else {
			cpsr &= ~CPSR::Carry;
		}
	};

	const auto write_reg = [&](u32 index, u32 value) {
		if (index == PC_INDEX) {
			gprs[PC_INDEX] = value & ~1u;
			if (value & 1u) {
				cpsr |= CPSR::Thumb;
			}
		} else {
			gprs[index] = value;
		}
	};

	const auto clear_exclusive = [&]() {
		exclusiveValid = false;
		exclusiveAddress = 0;
		exclusiveSize = 0;
	};

	if ((inst & 0x0F000000u) == 0x0F000000u) {
		const u32 svc = inst & 0x00FFFFFFu;
		gprs[PC_INDEX] = old_pc + 4;
		kernel.serviceSVC(svc);
		return 4;
	}

	if ((inst & 0x0FFFFFF0u) == 0x012FFF10u) {
		const u32 rm = inst & 0xF;
		const u32 target = get_reg_operand(rm);
		if (inst & (1u << 5)) {
			gprs[LR_INDEX] = old_pc + 4;
		}
		gprs[PC_INDEX] = target & ~1u;
		if (target & 1u) cpsr |= CPSR::Thumb;
		else cpsr &= ~CPSR::Thumb;
		return 3;
	}

	// BLX (immediate)
	if ((inst & 0xFE000000u) == 0xFA000000u) {
		const u32 h = (inst >> 24) & 1u;
		const u32 imm24 = inst & 0x00FFFFFFu;
		const u32 target = (old_pc + 8 + (signExtend(imm24, 24) << 2) + (h << 1)) & ~1u;
		gprs[LR_INDEX] = old_pc + 4;
		gprs[PC_INDEX] = target;
		cpsr |= CPSR::Thumb;
		return 3;
	}

	if ((inst & 0x0E000000u) == 0x0A000000u) {
		u32 imm24 = inst & 0x00FFFFFFu;
		const u32 target = old_pc + 8 + (signExtend(imm24, 24) << 2);
		if (inst & (1u << 24)) {
			gprs[LR_INDEX] = old_pc + 4;
		}
		gprs[PC_INDEX] = target;
		return 3;
	}

	if ((inst & 0x0FC000F0u) == 0x00000090u) {
		const bool accumulate = (inst & (1u << 21)) != 0;
		const bool set_flags = (inst & (1u << 20)) != 0;
		const u32 rd = (inst >> 16) & 0xF;
		const u32 rn = (inst >> 12) & 0xF;
		const u32 rs = (inst >> 8) & 0xF;
		const u32 rm = inst & 0xF;
		u32 result = get_reg_operand(rm) * get_reg_operand(rs);
		if (accumulate) {
			result += get_reg_operand(rn);
		}
		write_reg(rd, result);
		if (set_flags) {
			setNZFlags(result);
		}
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	// LDREX/STREX family (ARMv6K)
	if ((inst & 0x000000F0u) == 0x90u && ((inst >> 20) & 0xFFu) >= 0x18u && ((inst >> 20) & 0xFFu) <= 0x1Fu) {
		const u32 op = (inst >> 20) & 0xFFu;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 base = get_reg_operand(rn);
		const u32 rm = inst & 0xF;

		switch (op) {
			case 0x19: {  // LDREX
				write_reg(rd, mem.read32(base));
				exclusiveAddress = base;
				exclusiveSize = 4;
				exclusiveValid = true;
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			case 0x18: {  // STREX
				const bool success = exclusiveValid && exclusiveAddress == base && exclusiveSize == 4;
				if (success) {
					mem.write32(base, get_reg_operand(rm));
					write_reg(rd, 0);
				} else {
					write_reg(rd, 1);
				}
				clear_exclusive();
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			case 0x1D: {  // LDREXB
				write_reg(rd, mem.read8(base));
				exclusiveAddress = base;
				exclusiveSize = 1;
				exclusiveValid = true;
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			case 0x1C: {  // STREXB
				const bool success = exclusiveValid && exclusiveAddress == base && exclusiveSize == 1;
				if (success) {
					mem.write8(base, static_cast<u8>(get_reg_operand(rm)));
					write_reg(rd, 0);
				} else {
					write_reg(rd, 1);
				}
				clear_exclusive();
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			case 0x1F: {  // LDREXH
				write_reg(rd, mem.read16(base));
				exclusiveAddress = base;
				exclusiveSize = 2;
				exclusiveValid = true;
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			case 0x1E: {  // STREXH
				const bool success = exclusiveValid && exclusiveAddress == base && exclusiveSize == 2;
				if (success) {
					mem.write16(base, static_cast<u16>(get_reg_operand(rm)));
					write_reg(rd, 0);
				} else {
					write_reg(rd, 1);
				}
				clear_exclusive();
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 3;
			}
			default: break;
		}
	}

	// CLZ
	if ((inst & 0x0FFF0FF0u) == 0x016F0F10u) {
		const u32 rd = (inst >> 12) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 value = get_reg_operand(rm);
		write_reg(rd, static_cast<u32>(std::countl_zero(value)));
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// SXTB/SXTH/UXTB/UXTH
	if ((inst & 0x000000F0u) == 0x70u) {
		const u32 key = (inst >> 16) & 0xFFFu;
		if (key == 0x6AFu || key == 0x6BFu || key == 0x6EFu || key == 0x6FFu) {
			const u32 rd = (inst >> 12) & 0xF;
			const u32 rm = inst & 0xF;
			const u32 rotate = ((inst >> 10) & 0x3) * 8;
			const u32 value = ror32(get_reg_operand(rm), rotate);
			u32 result = value;
			switch (key) {
				case 0x6AFu: result = static_cast<u32>(static_cast<s32>(static_cast<s8>(value & 0xFFu))); break;   // SXTB
				case 0x6BFu: result = static_cast<u32>(static_cast<s32>(static_cast<s16>(value & 0xFFFFu))); break; // SXTH
				case 0x6EFu: result = value & 0xFFu; break;   // UXTB
				case 0x6FFu: result = value & 0xFFFFu; break; // UXTH
				default: break;
			}
			write_reg(rd, result);
			if (rd != PC_INDEX) {
				gprs[PC_INDEX] = old_pc + 4;
			}
			return 1;
		}
	}

	// QADD/QSUB/QDADD/QDSUB
	if ((inst & 0x0F0000F0u) == 0x01000050u) {
		const u32 op = (inst >> 21) & 0x3;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 rm = inst & 0xF;
		const auto saturating = [&](s64 value) {
			if (value > static_cast<s64>(0x7FFFFFFF)) {
				cpsr |= CPSR::StickyOverflow;
				return static_cast<u32>(0x7FFFFFFF);
			}
			if (value < static_cast<s64>(-0x80000000ll)) {
				cpsr |= CPSR::StickyOverflow;
				return static_cast<u32>(0x80000000u);
			}
			return static_cast<u32>(static_cast<s32>(value));
		};

		s64 lhs = static_cast<s32>(get_reg_operand(rm));
		s64 rhs = static_cast<s32>(get_reg_operand(rn));
		if (op == 0x2 || op == 0x3) {
			rhs = static_cast<s64>(static_cast<s32>(saturating(rhs * 2)));
		}

		u32 result = 0;
		switch (op) {
			case 0x0: result = saturating(lhs + rhs); break; // QADD
			case 0x1: result = saturating(lhs - rhs); break; // QSUB
			case 0x2: result = saturating(lhs + rhs); break; // QDADD
			case 0x3: result = saturating(lhs - rhs); break; // QDSUB
		}
		write_reg(rd, result);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	if ((inst & 0x0FB00FF0u) == 0x01000090u) {
		const bool byte = (inst & (1u << 22)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 addr = get_reg_operand(rn);
		if (byte) {
			u8 value = mem.read8(addr);
			mem.write8(addr, static_cast<u8>(get_reg_operand(rm)));
			clear_exclusive();
			write_reg(rd, value);
		} else {
			u32 value = mem.read32(addr);
			mem.write32(addr, get_reg_operand(rm));
			clear_exclusive();
			write_reg(rd, value);
		}
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 3;
	}

	if ((inst & 0x0FBF0FFFu) == 0x010F0000u) {
		const u32 rd = (inst >> 12) & 0xF;
		write_reg(rd, cpsr);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	if ((inst & 0x0DBFF000u) == 0x0128F000u) {
		const bool flags_only = (inst & (1u << 16)) == 0;
		u32 operand = 0;
		if (inst & (1u << 25)) {
			const u32 imm = inst & 0xFF;
			const u32 rot = ((inst >> 8) & 0xF) * 2;
			operand = ror32(imm, rot);
		} else {
			operand = get_reg_operand(inst & 0xF);
		}
		if (flags_only) {
			cpsr = (cpsr & ~0xF0000000u) | (operand & 0xF0000000u);
		} else {
			cpsr = operand;
		}
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// Coprocessor register transfer (MRC/MCR) - implement required CP15 subset used by HLE/userland.
	if ((inst & 0x0F000010u) == 0x0E000010u) {
		const bool load = (inst & (1u << 20)) != 0;  // 1=MRC, 0=MCR
		const u32 crn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 coproc = (inst >> 8) & 0xF;
		const u32 opc2 = (inst >> 5) & 0x7;
		const u32 crm = inst & 0xF;
		const u32 opc1 = (inst >> 21) & 0x7;

		if (coproc == 15) {
			// TPIDRURW/TPIDRURO-style TLS register accesses used by many binaries.
			if (crn == 13 && crm == 0 && opc1 == 0 && opc2 == 2) {
				if (load) write_reg(rd, tlsBase);
				else tlsBase = get_reg_operand(rd);
				if (rd != PC_INDEX || !load) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}
			if (crn == 13 && crm == 0 && opc1 == 0 && opc2 == 3) {
				if (load) write_reg(rd, tlsBase);
				else tlsBase = get_reg_operand(rd);
				if (rd != PC_INDEX || !load) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}

			// CP15 barrier/cache maintenance instructions are accepted as no-ops in HLE interpreter path.
			if (!load) {
				clear_exclusive();
			}
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}
	}

	// Halfword/signed data transfer (LDRH/LDRSH/LDRSB/STRH)
	if ((inst & 0x0E000090u) == 0x00000090u && (inst & 0x0C000000u) == 0x00000000u) {
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool immediate = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 sh = (inst >> 5) & 0x3;

		u32 offset = 0;
		if (immediate) {
			offset = ((inst >> 8) & 0xF) << 4;
			offset |= inst & 0xF;
		} else {
			offset = get_reg_operand(inst & 0xF);
		}

		const u32 base = get_reg_operand(rn);
		u32 effective = pre ? (up ? base + offset : base - offset) : base;

		if (!load && sh == 0x1) {  // STRH
			mem.write16(effective, static_cast<u16>(gprs[rd]));
			clear_exclusive();
		} else if (load && sh == 0x1) {  // LDRH
			write_reg(rd, mem.read16(effective));
		} else if (load && sh == 0x2) {  // LDRSB
			const s8 value = static_cast<s8>(mem.read8(effective));
			write_reg(rd, static_cast<u32>(static_cast<s32>(value)));
		} else if (load && sh == 0x3) {  // LDRSH
			const s16 value = static_cast<s16>(mem.read16(effective));
			write_reg(rd, static_cast<u32>(static_cast<s32>(value)));
		} else {
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		if (!pre) {
			effective = up ? base + offset : base - offset;
		}
		if (write_back || !pre) {
			gprs[rn] = effective;
		}
		if (!(load && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	if ((inst & 0x0E000000u) == 0x08000000u) {
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool psr = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 reg_list = inst & 0xFFFF;
		if (psr || reg_list == 0) {
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		u32 count = std::popcount(reg_list);
		u32 base = get_reg_operand(rn);
		u32 addr = base;
		if (up) {
			if (pre) {
				addr += 4;
			}
		} else {
			addr = base - (count * 4);
			if (!pre) {
				addr += 4;
			}
		}

		for (u32 reg = 0; reg < 16; reg++) {
			if ((reg_list & (1u << reg)) == 0) {
				continue;
			}
			if (load) {
				write_reg(reg, mem.read32(addr));
			} else {
				const u32 value = (reg == PC_INDEX) ? (old_pc + 12) : gprs[reg];
				mem.write32(addr, value);
				clear_exclusive();
			}
			addr += 4;
		}

		if (write_back) {
			gprs[rn] = up ? (base + count * 4) : (base - count * 4);
		}

		if ((load && (reg_list & (1u << PC_INDEX)) == 0) || !load) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2 + count;
	}

	if ((inst & 0x0C000000u) == 0x04000000u) {
		const bool immediate = (inst & (1u << 25)) == 0;
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool byte = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		u32 offset = 0;
		if (immediate) {
			offset = inst & 0xFFF;
		} else {
			const u32 rm = inst & 0xF;
			const u32 shift_type = (inst >> 5) & 0x3;
			const u32 shift_imm = (inst >> 7) & 0x1F;
			u32 rm_val = get_reg_operand(rm);
			switch (shift_type) {
				case 0: offset = rm_val << shift_imm; break;
				case 1: offset = (shift_imm == 0) ? 0 : (rm_val >> shift_imm); break;
				case 2: offset = (shift_imm == 0) ? (((rm_val >> 31) & 1u) ? 0xFFFFFFFFu : 0u)
										 : static_cast<u32>(static_cast<s32>(rm_val) >> shift_imm);
						break;
				case 3: offset = (shift_imm == 0) ? ((cpsr & CPSR::Carry) ? 0x80000000u : 0u) | (rm_val >> 1)
										 : ror32(rm_val, shift_imm);
						break;
			}
		}

		const u32 base = get_reg_operand(rn);
		u32 effective = base;
		if (pre) {
			effective = up ? (base + offset) : (base - offset);
		}

			if (load) {
				u32 value = byte ? static_cast<u32>(mem.read8(effective)) : mem.read32(effective);
				write_reg(rd, value);
			} else {
				const u32 value = (rd == PC_INDEX) ? old_pc + 12 : gprs[rd];
				if (byte) mem.write8(effective, static_cast<u8>(value));
				else mem.write32(effective, value);
				clear_exclusive();
			}

		if (!pre) {
			effective = up ? (base + offset) : (base - offset);
		}
		if (write_back || !pre) {
			gprs[rn] = effective;
		}

		if (!(load && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	if ((inst & 0x0C000000u) == 0x00000000u) {
		const bool immediate = (inst & (1u << 25)) != 0;
		const u32 opcode = (inst >> 21) & 0xF;
		const bool set_flags = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		bool shifter_carry = (cpsr & CPSR::Carry) != 0;
		u32 op2 = 0;
		if (immediate) {
			const u32 imm8 = inst & 0xFF;
			const u32 rot = ((inst >> 8) & 0xF) * 2;
			op2 = ror32(imm8, rot);
			if (rot != 0) {
				shifter_carry = (op2 >> 31) != 0;
			}
		} else {
			u32 rm_val = get_reg_operand(inst & 0xF);
			const bool by_register = (inst & (1u << 4)) != 0;
			const u32 shift_type = (inst >> 5) & 0x3;
			u32 shift_amount = 0;
			if (by_register) {
				shift_amount = get_reg_operand((inst >> 8) & 0xF) & 0xFF;
			} else {
				shift_amount = (inst >> 7) & 0x1F;
			}

			switch (shift_type) {
				case 0:  // LSL
					if (shift_amount == 0) {
						op2 = rm_val;
					} else if (shift_amount < 32) {
						op2 = rm_val << shift_amount;
						shifter_carry = ((rm_val >> (32 - shift_amount)) & 1u) != 0;
					} else if (shift_amount == 32) {
						op2 = 0;
						shifter_carry = (rm_val & 1u) != 0;
					} else {
						op2 = 0;
						shifter_carry = false;
					}
					break;
				case 1:  // LSR
					if (shift_amount == 0) {
						shift_amount = by_register ? 32 : 32;
					}
					if (shift_amount < 32) {
						op2 = rm_val >> shift_amount;
						shifter_carry = ((rm_val >> (shift_amount - 1)) & 1u) != 0;
					} else if (shift_amount == 32) {
						op2 = 0;
						shifter_carry = (rm_val >> 31) != 0;
					} else {
						op2 = 0;
						shifter_carry = false;
					}
					break;
				case 2:  // ASR
					if (shift_amount == 0) {
						shift_amount = by_register ? 32 : 32;
					}
					if (shift_amount < 32) {
						op2 = static_cast<u32>(static_cast<s32>(rm_val) >> shift_amount);
						shifter_carry = ((rm_val >> (shift_amount - 1)) & 1u) != 0;
					} else {
						op2 = (rm_val & 0x80000000u) ? 0xFFFFFFFFu : 0;
						shifter_carry = (rm_val >> 31) != 0;
					}
					break;
				case 3:  // ROR/RRX
					if (!by_register && shift_amount == 0) {
						op2 = ((cpsr & CPSR::Carry) ? 0x80000000u : 0u) | (rm_val >> 1);
						shifter_carry = (rm_val & 1u) != 0;
					} else {
						shift_amount &= 31;
						if (shift_amount == 0) {
							op2 = rm_val;
							shifter_carry = (rm_val >> 31) != 0;
						} else {
							op2 = ror32(rm_val, shift_amount);
							shifter_carry = (op2 >> 31) != 0;
						}
					}
					break;
			}
		}

		u32 lhs = get_reg_operand(rn);
		u32 result = 0;
		bool write_result = true;
		switch (opcode) {
			case 0x0: result = lhs & op2; break;                     // AND
			case 0x1: result = lhs ^ op2; break;                     // EOR
			case 0x2: result = lhs - op2; break;                     // SUB
			case 0x3: result = op2 - lhs; break;                     // RSB
			case 0x4: result = lhs + op2; break;                     // ADD
			case 0x5: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs + op2 + carry;
				break;
			}
			case 0x6: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs - op2 - (1u - carry);
				break;
			}
			case 0x7: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = op2 - lhs - (1u - carry);
				break;
			}
			case 0x8: result = lhs & op2; write_result = false; break;  // TST
			case 0x9: result = lhs ^ op2; write_result = false; break;  // TEQ
			case 0xA: result = lhs - op2; write_result = false; break;  // CMP
			case 0xB: result = lhs + op2; write_result = false; break;  // CMN
			case 0xC: result = lhs | op2; break;                      // ORR
			case 0xD: result = op2; break;                            // MOV
			case 0xE: result = lhs & ~op2; break;                     // BIC
			case 0xF: result = ~op2; break;                           // MVN
		}

		if (set_flags || !write_result) {
			switch (opcode) {
				case 0x0:
				case 0x1:
				case 0x8:
				case 0x9:
				case 0xC:
				case 0xD:
				case 0xE:
				case 0xF:
					setNZFlags(result);
					set_carry(shifter_carry);
					break;
				case 0x2:
				case 0xA:
					setSubFlags(lhs, op2, result);
					break;
				case 0x3:
					setSubFlags(op2, lhs, result);
					break;
				case 0x4:
				case 0xB:
					setAddFlags(lhs, op2, result);
					break;
				case 0x5: {
					const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
					const u64 wide = static_cast<u64>(lhs) + static_cast<u64>(op2) + carry;
					setNZFlags(result);
					set_carry((wide >> 32) != 0);
					const bool overflow = ((~(lhs ^ op2) & (lhs ^ result)) & 0x80000000u) != 0;
					if (overflow) cpsr |= CPSR::Overflow;
					else cpsr &= ~CPSR::Overflow;
					break;
				}
				case 0x6:
				case 0x7: {
					const u32 rhs = (opcode == 0x6) ? op2 : lhs;
					const u32 l = (opcode == 0x6) ? lhs : op2;
					setNZFlags(result);
					const u64 sub = static_cast<u64>(l) - static_cast<u64>(rhs) - ((cpsr & CPSR::Carry) ? 0u : 1u);
					set_carry((sub >> 63) == 0);
					const bool overflow = (((l ^ rhs) & (l ^ result)) & 0x80000000u) != 0;
					if (overflow) cpsr |= CPSR::Overflow;
					else cpsr &= ~CPSR::Overflow;
					break;
				}
			}
		}

		if (write_result) {
			write_reg(rd, result);
		}

		if (!(write_result && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	gprs[PC_INDEX] = old_pc + 4;
	return 1;
}

u32 CPU::executeThumb(u16 inst) {
	const u32 old_pc = gprs[PC_INDEX];
	const auto set_carry = [&](bool value) {
		if (value) cpsr |= CPSR::Carry;
		else cpsr &= ~CPSR::Carry;
	};
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
			exclusiveValid = false;
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
					exclusiveValid = false;
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
					exclusiveValid = false;
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

	gprs[PC_INDEX] = old_pc + 2;
	return 1;
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
			if (consumed >= ticksLeft) {
				ticksLeft = 0;
			} else {
				ticksLeft -= consumed;
			}
		}

		emu.pollScheduler();
	}
}
