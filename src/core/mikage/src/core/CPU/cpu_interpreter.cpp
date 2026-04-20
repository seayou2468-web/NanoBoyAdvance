#include "cpu_interpreter.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <optional>

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
	exclusiveAddress = 0;
	exclusiveSize = 0;
	exclusiveValid = false;
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

	// MLS
	if ((inst & 0x0FF000F0u) == 0x00600090u) {
		const u32 rd = (inst >> 16) & 0xF;
		const u32 ra = (inst >> 12) & 0xF;
		const u32 rs = (inst >> 8) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 product = get_reg_operand(rm) * get_reg_operand(rs);
		write_reg(rd, get_reg_operand(ra) - product);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	// UMULL/UMLAL/SMULL/SMLAL
	if ((inst & 0x0F8000F0u) == 0x00800090u) {
		const bool signed_mul = (inst & (1u << 22)) != 0;
		const bool accumulate = (inst & (1u << 21)) != 0;
		const bool set_flags = (inst & (1u << 20)) != 0;
		const u32 rd_hi = (inst >> 16) & 0xF;
		const u32 rd_lo = (inst >> 12) & 0xF;
		const u32 rs = (inst >> 8) & 0xF;
		const u32 rm = inst & 0xF;

		u64 result = 0;
		if (signed_mul) {
			const s64 lhs = static_cast<s32>(get_reg_operand(rm));
			const s64 rhs = static_cast<s32>(get_reg_operand(rs));
			result = static_cast<u64>(lhs * rhs);
			if (accumulate) {
				const u64 acc = (static_cast<u64>(gprs[rd_hi]) << 32) | gprs[rd_lo];
				result += acc;
			}
		} else {
			result = static_cast<u64>(get_reg_operand(rm)) * static_cast<u64>(get_reg_operand(rs));
			if (accumulate) {
				const u64 acc = (static_cast<u64>(gprs[rd_hi]) << 32) | gprs[rd_lo];
				result += acc;
			}
		}

		write_reg(rd_lo, static_cast<u32>(result));
		write_reg(rd_hi, static_cast<u32>(result >> 32));
		if (set_flags) {
			cpsr = (cpsr & ~(CPSR::Sign | CPSR::Zero)) |
				   ((result >> 63) ? CPSR::Sign : 0) |
				   ((result == 0) ? CPSR::Zero : 0);
		}
		if (rd_hi != PC_INDEX && rd_lo != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 3;
	}

	// SDIV/UDIV
	if ((inst & 0x0FE0F0F0u) == 0x0710F010u || (inst & 0x0FE0F0F0u) == 0x0730F010u) {
		const bool is_unsigned = (inst & 0x00200000u) != 0;
		const u32 rd = (inst >> 16) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 rn = (inst >> 8) & 0xF;
		u32 result = 0;
		if (get_reg_operand(rm) != 0) {
			if (is_unsigned) {
				result = get_reg_operand(rn) / get_reg_operand(rm);
			} else {
				result = static_cast<u32>(static_cast<s32>(get_reg_operand(rn)) / static_cast<s32>(get_reg_operand(rm)));
			}
		}
		write_reg(rd, result);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 4;
	}

	// MOVW/MOVT
	if ((inst & 0x0FF00000u) == 0x03000000u || (inst & 0x0FF00000u) == 0x03400000u) {
		const bool top = (inst & 0x00400000u) != 0;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 imm16 = ((inst >> 4) & 0xF000u) | (inst & 0x0FFFu);
		const u32 value = top ? ((gprs[rd] & 0x0000FFFFu) | (imm16 << 16)) : imm16;
		write_reg(rd, value);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// SBFX/UBFX
	if ((inst & 0x0FE00070u) == 0x07A00050u || (inst & 0x0FE00070u) == 0x07E00050u) {
		const bool unsigned_extract = (inst & 0x00400000u) != 0;
		const u32 widthm1 = (inst >> 16) & 0x1F;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 lsb = (inst >> 7) & 0x1F;
		const u32 rn = inst & 0xF;
		const u32 width = widthm1 + 1;

		const u32 value = get_reg_operand(rn) >> lsb;
		const u32 mask = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
		u32 result = value & mask;
		if (!unsigned_extract && width < 32 && ((result >> (width - 1)) & 1u) != 0) {
			result |= ~mask;
		}
		write_reg(rd, result);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// BFI/BFC
	if ((inst & 0x0FE00070u) == 0x07C00010u) {
		const u32 msb = (inst >> 16) & 0x1F;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 lsb = (inst >> 7) & 0x1F;
		const u32 rn = inst & 0xF;
		if (msb >= lsb) {
			const u32 width = msb - lsb + 1;
			const u32 source = (rn == 0xFu) ? 0u : get_reg_operand(rn);  // BFC uses Rn=1111.
			const u32 field_mask = (width >= 32) ? 0xFFFFFFFFu : (((1u << width) - 1u) << lsb);
			const u32 result = (gprs[rd] & ~field_mask) | ((source << lsb) & field_mask);
			write_reg(rd, result);
		}
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
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
			case 0x1B: {  // LDREXD
				if ((rd & 1u) == 0 && rd != LR_INDEX) {
					write_reg(rd, mem.read32(base));
					write_reg(rd + 1, mem.read32(base + 4));
				}
				exclusiveAddress = base;
				exclusiveSize = 8;
				exclusiveValid = true;
				if (rd != PC_INDEX && rd + 1 != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 4;
			}
			case 0x1A: {  // STREXD
				const bool success = exclusiveValid && exclusiveAddress == base && exclusiveSize == 8;
				if (success && (rm & 1u) == 0 && rm != LR_INDEX) {
					mem.write32(base, gprs[rm]);
					mem.write32(base + 4, gprs[rm + 1]);
					write_reg(rd, 0);
				} else {
					write_reg(rd, 1);
				}
				clear_exclusive();
				if (rd != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 4;
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

		if (key == 0x6A7u || key == 0x6B7u || key == 0x6E7u || key == 0x6F7u) {
			const u32 rn = (inst >> 16) & 0xF;
			const u32 rd = (inst >> 12) & 0xF;
			const u32 rm = inst & 0xF;
			const u32 rotate = ((inst >> 10) & 0x3) * 8;
			const u32 value = ror32(get_reg_operand(rm), rotate);
			u32 ext = 0;
			switch (key) {
				case 0x6A7u: ext = static_cast<u32>(static_cast<s32>(static_cast<s8>(value & 0xFFu))); break;   // SXTAB
				case 0x6B7u: ext = static_cast<u32>(static_cast<s32>(static_cast<s16>(value & 0xFFFFu))); break; // SXTAH
				case 0x6E7u: ext = value & 0xFFu; break;   // UXTAB
				case 0x6F7u: ext = value & 0xFFFFu; break; // UXTAH
				default: break;
			}
			write_reg(rd, get_reg_operand(rn) + ext);
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

	// SSAT/USAT
	if ((inst & 0x0FA00010u) == 0x06A00010u || (inst & 0x0FA00010u) == 0x06E00010u) {
		const bool is_unsigned = (inst & 0x00400000u) != 0;
		const u32 sat_imm = (inst >> 16) & 0x1F;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 shift_imm = (inst >> 7) & 0x1F;
		const bool arithmetic_shift = (inst & (1u << 6)) != 0;
		const u32 rn = inst & 0xF;

		u32 shifted = get_reg_operand(rn);
		if (arithmetic_shift) {
			if (shift_imm == 0) {
				shifted = shifted;
			} else {
				shifted = static_cast<u32>(static_cast<s32>(shifted) >> shift_imm);
			}
		} else {
			shifted <<= shift_imm;
		}

		bool saturated = false;
		u32 result = shifted;
		if (is_unsigned) {
			const u32 sat = sat_imm + 1;
			const u32 max_value = (sat >= 32) ? 0xFFFFFFFFu : ((1u << sat) - 1u);
			const s64 value = static_cast<s32>(shifted);
			if (value < 0) {
				result = 0;
				saturated = true;
			} else if (static_cast<u64>(value) > max_value) {
				result = max_value;
				saturated = true;
			}
		} else {
			const u32 sat = sat_imm + 1;
			const s64 min_value = -(static_cast<s64>(1) << (sat - 1));
			const s64 max_value = (static_cast<s64>(1) << (sat - 1)) - 1;
			const s64 value = static_cast<s32>(shifted);
			if (value < min_value) {
				result = static_cast<u32>(static_cast<s32>(min_value));
				saturated = true;
			} else if (value > max_value) {
				result = static_cast<u32>(static_cast<s32>(max_value));
				saturated = true;
			}
		}
		if (saturated) {
			cpsr |= CPSR::StickyOverflow;
		}
		write_reg(rd, result);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// PKHBT/PKHTB
	// A32 encoding class: cond 0110 1000 0/1 Rn Rd imm5 tb 0 Rm
	if ((inst & 0x0FF00010u) == 0x06800010u) {
		const bool top = (inst & (1u << 6)) != 0;  // 0 = PKHBT, 1 = PKHTB
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 imm5 = (inst >> 7) & 0x1F;
		const u32 rm = inst & 0xF;

		u32 operand2 = get_reg_operand(rm);
		if (top) {
			// PKHTB uses arithmetic right shift, with imm==0 meaning ASR #32.
			if (imm5 == 0) {
				operand2 = (operand2 & 0x80000000u) ? 0xFFFFFFFFu : 0u;
			} else {
				operand2 = static_cast<u32>(static_cast<s32>(operand2) >> imm5);
			}
			write_reg(rd, (get_reg_operand(rn) & 0xFFFF0000u) | (operand2 & 0x0000FFFFu));
		} else {
			// PKHBT uses left shift.
			operand2 <<= imm5;
			write_reg(rd, (get_reg_operand(rn) & 0x0000FFFFu) | (operand2 & 0xFFFF0000u));
		}

		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// SEL (byte-wise select using GE[3:0] flags in CPSR)
	if ((inst & 0x0FF00FF0u) == 0x06800FB0u) {
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 ge = (cpsr >> 16) & 0xF;
		const u32 lhs = get_reg_operand(rn);
		const u32 rhs = get_reg_operand(rm);

		u32 result = 0;
		for (u32 lane = 0; lane < 4; lane++) {
			const u32 shift = lane * 8;
			const u32 mask = 0xFFu << shift;
			const bool take_lhs = (ge & (1u << lane)) != 0;
			result |= take_lhs ? (lhs & mask) : (rhs & mask);
		}

		write_reg(rd, result);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// USAD8 / USADA8 (sum of absolute byte differences)
	if ((inst & 0x0FF000F0u) == 0x07800010u) {
		const bool accumulate = ((inst >> 21) & 1u) != 0;  // 0=USAD8, 1=USADA8
		const u32 rd = (inst >> 16) & 0xF;
		const u32 ra = (inst >> 12) & 0xF;
		const u32 rm = (inst >> 8) & 0xF;
		const u32 rn = inst & 0xF;
		const u32 lhs = get_reg_operand(rn);
		const u32 rhs = get_reg_operand(rm);

		u32 sad = 0;
		for (u32 lane = 0; lane < 4; lane++) {
			const s32 a = static_cast<s32>((lhs >> (lane * 8)) & 0xFFu);
			const s32 b = static_cast<s32>((rhs >> (lane * 8)) & 0xFFu);
			sad += static_cast<u32>(std::abs(a - b));
		}
		if (accumulate) {
			sad += get_reg_operand(ra);
		}

		write_reg(rd, sad);
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 1;
	}

	// Parallel add/sub family (8-bit + 16-bit subsets).
	// These encodings are in the ARM media instruction space and use Rd/Rn/Rm with opcode-select in [24:20] and [7:4].
	if ((inst & 0x0FF00FF0u) == 0x06100F90u || (inst & 0x0FF00FF0u) == 0x06100FF0u || (inst & 0x0FF00FF0u) == 0x06500F90u ||
		(inst & 0x0FF00FF0u) == 0x06500FF0u || (inst & 0x0FF00FF0u) == 0x06700F90u || (inst & 0x0FF00FF0u) == 0x06700FF0u ||
		(inst & 0x0FF00FF0u) == 0x06600F90u || (inst & 0x0FF00FF0u) == 0x06600FF0u || (inst & 0x0FF00FF0u) == 0x06100F10u ||
		(inst & 0x0FF00FF0u) == 0x06100F70u || (inst & 0x0FF00FF0u) == 0x06500F10u || (inst & 0x0FF00FF0u) == 0x06500F70u ||
		(inst & 0x0FF00FF0u) == 0x06700F10u || (inst & 0x0FF00FF0u) == 0x06700F70u || (inst & 0x0FF00FF0u) == 0x06600F10u ||
		(inst & 0x0FF00FF0u) == 0x06600F70u) {
		const u32 rd = (inst >> 12) & 0xF;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rm = inst & 0xF;
		const u32 lhs = get_reg_operand(rn);
		const u32 rhs = get_reg_operand(rm);
		const u32 key = inst & 0x0FF00FF0u;
		u32 result = 0;

		const auto write_ge_lane = [&](u32 lane, bool ge) {
			const u32 ge_bit = 16u + lane;
			if (ge) cpsr |= (1u << ge_bit);
			else cpsr &= ~(1u << ge_bit);
		};

		// 16-bit lanes
		if (key == 0x06100F10u || key == 0x06100F70u || key == 0x06500F10u || key == 0x06500F70u || key == 0x06700F10u ||
			key == 0x06700F70u || key == 0x06600F10u || key == 0x06600F70u) {
			for (u32 lane = 0; lane < 2; lane++) {
				const u32 shift = lane * 16;
				const u32 a = (lhs >> shift) & 0xFFFFu;
				const u32 b = (rhs >> shift) & 0xFFFFu;
				u32 out = 0;

				if (key == 0x06100F10u) {  // SADD16
					const s32 sum = static_cast<s32>(static_cast<s16>(a)) + static_cast<s32>(static_cast<s16>(b));
					out = static_cast<u32>(sum) & 0xFFFFu;
					write_ge_lane(lane * 2, sum >= 0);
					write_ge_lane(lane * 2 + 1, sum >= 0);
				} else if (key == 0x06100F70u) {  // SSUB16
					const s32 diff = static_cast<s32>(static_cast<s16>(a)) - static_cast<s32>(static_cast<s16>(b));
					out = static_cast<u32>(diff) & 0xFFFFu;
					write_ge_lane(lane * 2, diff >= 0);
					write_ge_lane(lane * 2 + 1, diff >= 0);
				} else if (key == 0x06500F10u) {  // UADD16
					const u32 sum = a + b;
					out = sum & 0xFFFFu;
					write_ge_lane(lane * 2, sum > 0xFFFFu);
					write_ge_lane(lane * 2 + 1, sum > 0xFFFFu);
				} else if (key == 0x06500F70u) {  // USUB16
					const u32 diff = (a - b) & 0x1FFFFu;
					out = diff & 0xFFFFu;
					write_ge_lane(lane * 2, a >= b);
					write_ge_lane(lane * 2 + 1, a >= b);
				} else if (key == 0x06700F10u) {  // UHADD16
					out = (a + b) >> 1;
				} else if (key == 0x06700F70u) {  // UHSUB16
					out = (a - b) >> 1;
					} else if (key == 0x06600F10u) {  // UQADD16
						const u32 sum = a + b;
						if (sum > 0xFFFFu) {
							out = 0xFFFFu;
							cpsr |= CPSR::StickyOverflow;
						} else {
							out = sum;
						}
					} else {  // 0x06600F70u => UQSUB16
						if (a < b) {
							out = 0;
							cpsr |= CPSR::StickyOverflow;
						} else {
							out = a - b;
						}
					}

				result |= (out & 0xFFFFu) << shift;
			}
		} else {
			// 8-bit lanes
			for (u32 lane = 0; lane < 4; lane++) {
				const u32 shift = lane * 8;
				const u32 a = (lhs >> shift) & 0xFFu;
				const u32 b = (rhs >> shift) & 0xFFu;
				u32 out = 0;

				if (key == 0x06100F90u) {  // SADD8
					const s32 sum = static_cast<s32>(static_cast<s8>(a)) + static_cast<s32>(static_cast<s8>(b));
					out = static_cast<u32>(sum) & 0xFFu;
					write_ge_lane(lane, sum >= 0);
				} else if (key == 0x06100FF0u) {  // SSUB8
					const s32 diff = static_cast<s32>(static_cast<s8>(a)) - static_cast<s32>(static_cast<s8>(b));
					out = static_cast<u32>(diff) & 0xFFu;
					write_ge_lane(lane, diff >= 0);
				} else if (key == 0x06500F90u) {  // UADD8
					const u32 sum = a + b;
					out = sum & 0xFFu;
					write_ge_lane(lane, sum > 0xFFu);
				} else if (key == 0x06500FF0u) {  // USUB8
					const u32 diff = (a - b) & 0x1FFu;
					out = diff & 0xFFu;
					write_ge_lane(lane, a >= b);
				} else if (key == 0x06700F90u) {  // UHADD8
					out = (a + b) >> 1;
				} else if (key == 0x06700FF0u) {  // UHSUB8
					out = (a - b) >> 1;
				} else if (key == 0x06600FF0u) {  // UQSUB8
					if (a < b) {
						out = 0;
						cpsr |= CPSR::StickyOverflow;
					} else {
						out = a - b;
					}
				} else {  // 0x06600F90u => UQADD8
					const u32 sum = a + b;
					if (sum > 0xFFu) {
						out = 0xFFu;
						cpsr |= CPSR::StickyOverflow;
					} else {
						out = sum;
					}
				}

				result |= (out & 0xFFu) << shift;
			}
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

	// VFP scalar arithmetic subset (single/double): VMUL/VADD/VSUB/VDIV.
	// Encoding references are aligned with ARM VFP data-processing register forms.
	if ((inst & 0x0FB00E10u) == 0x0E000A00u || (inst & 0x0FB00E10u) == 0x0E200A00u || (inst & 0x0FB00E10u) == 0x0E300A00u ||
		(inst & 0x0FB00E10u) == 0x0E800A00u) {
		const auto update_fp_exceptions = [&]() {
			const int ex = std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT);
			if (ex & FE_INVALID) fpscr |= (1u << 0);     // IOC
			if (ex & FE_DIVBYZERO) fpscr |= (1u << 1);   // DZC
			if (ex & FE_OVERFLOW) fpscr |= (1u << 2);    // OFC
			if (ex & FE_UNDERFLOW) fpscr |= (1u << 3);   // UFC
			if (ex & FE_INEXACT) fpscr |= (1u << 4);     // IXC
		};
		const auto apply_fpscr_rounding = [&]() {
			const int previous = std::fegetround();
			switch ((fpscr >> 22) & 0x3u) {
				case 0x0: std::fesetround(FE_TONEAREST); break;
				case 0x1: std::fesetround(FE_UPWARD); break;
				case 0x2: std::fesetround(FE_DOWNWARD); break;
				case 0x3: std::fesetround(FE_TOWARDZERO); break;
			}
			return previous;
		};
		const auto restore_rounding = [&](int previous) {
			std::fesetround(previous);
		};

		const bool is_mla_class = (inst & 0x0FB00E10u) == 0x0E000A00u;
		const bool is_mul = (inst & 0x0FB00E10u) == 0x0E200A00u;
		const bool is_div = (inst & 0x0FB00E10u) == 0x0E800A00u;
		const bool double_precision = (inst & (1u << 8)) != 0;
		if (!double_precision) {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 sn = (((inst >> 16) & 0xF) << 1) | ((inst >> 7) & 1u);
			const u32 sm = ((inst & 0xF) << 1) | ((inst >> 5) & 1u);
				if (sd < 32 && sn < 32 && sm < 32) {
						const float lhs = std::bit_cast<float>(extRegs[sn]);
						const float rhs = std::bit_cast<float>(extRegs[sm]);
						float result = lhs;
						const int previous_round = apply_fpscr_rounding();
						std::feclearexcept(FE_ALL_EXCEPT);
							if (is_div) {
								result = lhs / rhs;  // VDIV
						} else if (is_mla_class) {
						const float acc = std::bit_cast<float>(extRegs[sd]);
						const float product = lhs * rhs;
						result = (inst & (1u << 6)) ? (acc - product) : (acc + product);  // VMLS / VMLA
					} else if (is_mul) {
						result = lhs * rhs;  // VMUL / VNMUL
						if (inst & (1u << 6)) {
							result = -result;  // VNMUL
						}
					} else if ((inst & (1u << 6)) != 0) {
					result = lhs - rhs;  // VSUB
						} else {
							result = lhs + rhs;  // VADD
						}
						update_fp_exceptions();
						restore_rounding(previous_round);
						extRegs[sd] = std::bit_cast<u32>(result);
						gprs[PC_INDEX] = old_pc + 4;
						return 2;
			}
		} else {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 dn = ((inst >> 16) & 0xF) | (((inst >> 7) & 1u) << 4);
			const u32 dm = (inst & 0xF) | (((inst >> 5) & 1u) << 4);
				if (dd < 32 && dn < 32 && dm < 32) {
					const u64 lhs_bits = static_cast<u64>(extRegs[dn * 2]) | (static_cast<u64>(extRegs[dn * 2 + 1]) << 32);
						const u64 rhs_bits = static_cast<u64>(extRegs[dm * 2]) | (static_cast<u64>(extRegs[dm * 2 + 1]) << 32);
						const double lhs = std::bit_cast<double>(lhs_bits);
						const double rhs = std::bit_cast<double>(rhs_bits);
						double result = lhs;
						const int previous_round = apply_fpscr_rounding();
						std::feclearexcept(FE_ALL_EXCEPT);
							if (is_div) {
								result = lhs / rhs;  // VDIV.F64
						} else if (is_mla_class) {
						const u64 acc_bits = static_cast<u64>(extRegs[dd * 2]) | (static_cast<u64>(extRegs[dd * 2 + 1]) << 32);
						const double acc = std::bit_cast<double>(acc_bits);
						const double product = lhs * rhs;
						result = (inst & (1u << 6)) ? (acc - product) : (acc + product);  // VMLS.F64 / VMLA.F64
					} else if (is_mul) {
						result = lhs * rhs;  // VMUL.F64 / VNMUL.F64
						if (inst & (1u << 6)) {
							result = -result;  // VNMUL.F64
						}
					} else if ((inst & (1u << 6)) != 0) {
					result = lhs - rhs;  // VSUB.F64
						} else {
							result = lhs + rhs;  // VADD.F64
						}
						update_fp_exceptions();
						restore_rounding(previous_round);
						const u64 out = std::bit_cast<u64>(result);
						extRegs[dd * 2] = static_cast<u32>(out);
					extRegs[dd * 2 + 1] = static_cast<u32>(out >> 32);
				gprs[PC_INDEX] = old_pc + 4;
				return 4;
			}
		}
	}

	// VFP VMOV immediate (single/double scalar subset).
	if ((inst & 0x0FBF0F0Fu) == 0x0EB00A00u) {
		const bool double_precision = (inst & (1u << 8)) != 0;
		const u32 imm8 = (((inst >> 16) & 0xF) << 4) | (inst & 0xF);
		if (!double_precision) {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			if (sd < 32) {
				const u32 sign = (imm8 >> 7) & 1u;
				const u32 b = (imm8 >> 6) & 1u;
				const u32 exp = ((~b & 1u) << 7) | (b << 6) | (b << 5) | (b << 4) | (b << 3) | (b << 2) |
								((imm8 >> 4) & 0x3);
				const u32 frac = (imm8 & 0xFu) << 19;
				extRegs[sd] = (sign << 31) | (exp << 23) | frac;
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		} else {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			if (dd < 32) {
				const u64 sign = static_cast<u64>((imm8 >> 7) & 1u);
				const u64 b = static_cast<u64>((imm8 >> 6) & 1u);
				const u64 exp = ((~b & 1ull) << 10) | (b << 9) | (b << 8) | (b << 7) | (b << 6) | (b << 5) |
								(b << 4) | (b << 3) | (b << 2) | ((imm8 >> 4) & 0x3u);
				const u64 frac = static_cast<u64>(imm8 & 0xFu) << 48;
				const u64 value = (sign << 63) | (exp << 52) | frac;
				extRegs[dd * 2] = static_cast<u32>(value);
				extRegs[dd * 2 + 1] = static_cast<u32>(value >> 32);
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		}
	}

	// VCVT between single and double precision (VCVTBDS subset).
	// Pattern reference: cond 1110 1D11 0111 Vd-- 101X 11M0 Vm--
	if ((inst & 0x0FBF0ED0u) == 0x0EB70AC0u) {
		const bool to_double = (inst & (1u << 8)) != 0;
		if (to_double) {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 sm = ((inst & 0xF) << 1) | ((inst >> 5) & 1u);
			if (dd < 32 && sm < 32) {
				const float in = std::bit_cast<float>(extRegs[sm]);
				const double out = static_cast<double>(in);
				const u64 bits = std::bit_cast<u64>(out);
				extRegs[dd * 2] = static_cast<u32>(bits);
				extRegs[dd * 2 + 1] = static_cast<u32>(bits >> 32);
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		} else {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 dm = (inst & 0xF) | (((inst >> 5) & 1u) << 4);
			if (sd < 32 && dm < 32) {
				const u64 bits = static_cast<u64>(extRegs[dm * 2]) | (static_cast<u64>(extRegs[dm * 2 + 1]) << 32);
				const double in = std::bit_cast<double>(bits);
				const float out = static_cast<float>(in);
				extRegs[sd] = std::bit_cast<u32>(out);
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		}
	}

	// VFP VMOV register-to-register (single/double scalar subset).
	if ((inst & 0x0FBF0ED0u) == 0x0EB00A40u) {
		const bool double_precision = (inst & (1u << 8)) != 0;
		if (!double_precision) {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 sm = ((inst & 0xF) << 1) | ((inst >> 5) & 1u);
			if (sd < 32 && sm < 32) {
				extRegs[sd] = extRegs[sm];
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		} else {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 dm = (inst & 0xF) | (((inst >> 5) & 1u) << 4);
			if (dd < 32 && dm < 32) {
				extRegs[dd * 2] = extRegs[dm * 2];
				extRegs[dd * 2 + 1] = extRegs[dm * 2 + 1];
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		}
	}

	// VFP unary arithmetic subset: VABS / VNEG / VSQRT (single/double).
	const bool is_vabs = (inst & 0x0FBF0ED0u) == 0x0EB00AC0u;
	const bool is_vsqrt = (inst & 0x0FBF0ED0u) == 0x0EB10AC0u;
	const bool is_vneg = (inst & 0x0FBE0ED0u) == 0x0EB00A40u;
	if (is_vabs || is_vneg || is_vsqrt) {
		const auto update_fp_exceptions = [&]() {
			const int ex = std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT);
			if (ex & FE_INVALID) fpscr |= (1u << 0);     // IOC
			if (ex & FE_DIVBYZERO) fpscr |= (1u << 1);   // DZC
			if (ex & FE_OVERFLOW) fpscr |= (1u << 2);    // OFC
			if (ex & FE_UNDERFLOW) fpscr |= (1u << 3);   // UFC
			if (ex & FE_INEXACT) fpscr |= (1u << 4);     // IXC
		};
		const auto apply_fpscr_rounding = [&]() {
			const int previous = std::fegetround();
			switch ((fpscr >> 22) & 0x3u) {
				case 0x0: std::fesetround(FE_TONEAREST); break;
				case 0x1: std::fesetround(FE_UPWARD); break;
				case 0x2: std::fesetround(FE_DOWNWARD); break;
				case 0x3: std::fesetround(FE_TOWARDZERO); break;
			}
			return previous;
		};
		const auto restore_rounding = [&](int previous) {
			std::fesetround(previous);
		};

		const bool double_precision = (inst & (1u << 8)) != 0;
		if (!double_precision) {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 sm = ((inst & 0xF) << 1) | ((inst >> 5) & 1u);
			if (sd < 32 && sm < 32) {
				const float value = std::bit_cast<float>(extRegs[sm]);
				const int previous_round = apply_fpscr_rounding();
				std::feclearexcept(FE_ALL_EXCEPT);
				const float result = is_vsqrt ? std::sqrt(value) : (is_vneg ? -value : std::fabs(value));
				update_fp_exceptions();
				restore_rounding(previous_round);
				extRegs[sd] = std::bit_cast<u32>(result);
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		} else {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 dm = (inst & 0xF) | (((inst >> 5) & 1u) << 4);
			if (dd < 32 && dm < 32) {
				const u64 bits = static_cast<u64>(extRegs[dm * 2]) | (static_cast<u64>(extRegs[dm * 2 + 1]) << 32);
				const double value = std::bit_cast<double>(bits);
				const int previous_round = apply_fpscr_rounding();
				std::feclearexcept(FE_ALL_EXCEPT);
				const double result = is_vsqrt ? std::sqrt(value) : (is_vneg ? -value : std::fabs(value));
				update_fp_exceptions();
				restore_rounding(previous_round);
				const u64 out = std::bit_cast<u64>(result);
				extRegs[dd * 2] = static_cast<u32>(out);
				extRegs[dd * 2 + 1] = static_cast<u32>(out >> 32);
				gprs[PC_INDEX] = old_pc + 4;
				return 2;
			}
		}
	}

	// VFP compare subset: VCMP/VCMPE (single/double), updates FPSCR NZCV.
	if ((inst & 0x0FBF0E50u) == 0x0EB40A40u) {
		const bool double_precision = (inst & (1u << 8)) != 0;
		u32 nzcv = 0;
		if (!double_precision) {
			const u32 sd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 sm = ((inst & 0xF) << 1) | ((inst >> 5) & 1u);
			if (sd < 32 && sm < 32) {
					const float lhs = std::bit_cast<float>(extRegs[sd]);
					const float rhs = std::bit_cast<float>(extRegs[sm]);
					if (std::isnan(lhs) || std::isnan(rhs)) {
						nzcv = 0x30000000u;  // unordered
						fpscr |= (1u << 0);   // IOC
					} else if (lhs == rhs) {
					nzcv = 0x60000000u;  // Z=1,C=1
				} else if (lhs < rhs) {
					nzcv = 0x80000000u;  // N=1
				} else {
					nzcv = 0x20000000u;  // C=1
				}
				fpscr = (fpscr & ~0xF0000000u) | nzcv;
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		} else {
			const u32 dd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 dm = (inst & 0xF) | (((inst >> 5) & 1u) << 4);
			if (dd < 32 && dm < 32) {
				const u64 lhs_bits = static_cast<u64>(extRegs[dd * 2]) | (static_cast<u64>(extRegs[dd * 2 + 1]) << 32);
				const u64 rhs_bits = static_cast<u64>(extRegs[dm * 2]) | (static_cast<u64>(extRegs[dm * 2 + 1]) << 32);
					const double lhs = std::bit_cast<double>(lhs_bits);
					const double rhs = std::bit_cast<double>(rhs_bits);
					if (std::isnan(lhs) || std::isnan(rhs)) {
						nzcv = 0x30000000u;
						fpscr |= (1u << 0);
					} else if (lhs == rhs) {
					nzcv = 0x60000000u;
				} else if (lhs < rhs) {
					nzcv = 0x80000000u;
				} else {
					nzcv = 0x20000000u;
				}
				fpscr = (fpscr & ~0xF0000000u) | nzcv;
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}
		}
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
			const auto cp15_read = [&]() -> std::optional<u32> {
				// CPU ID / capability registers
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 0) return 0x410FC0F0u;  // MIDR (Cortex-A9-like)
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 1) return 0x0F0D2112u;  // CTR
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 5) return 0u;           // MPIDR (single-core view)

				// System control and MMU registers
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15SCTLR;
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15ACTLR;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15TTBR0;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15TTBR1;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 2) return cp15TTBCR;
				if (crn == 3 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DACR;

				// Fault status/address registers
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DFSR;
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15IFSR;
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DFAR;
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 2) return cp15IFAR;
				return std::nullopt;
			};
			const auto cp15_write = [&](u32 value) -> bool {
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15SCTLR = value;
					return true;
				}
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15ACTLR = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15TTBR0 = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15TTBR1 = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 2) {
					cp15TTBCR = value;
					return true;
				}
				if (crn == 3 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DACR = value;
					return true;
				}
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DFSR = value;
					return true;
				}
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15IFSR = value;
					return true;
				}
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DFAR = value;
					return true;
				}
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 2) {
					cp15IFAR = value;
					return true;
				}
				return false;
			};

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

			if (load) {
				const auto value = cp15_read();
				if (value.has_value()) {
					write_reg(rd, *value);
					if (rd != PC_INDEX) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			} else if (cp15_write(get_reg_operand(rd))) {
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}

			// CP15 barrier/cache maintenance instructions are accepted as no-ops in HLE interpreter path.
			if (!load) {
				clear_exclusive();
			}
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		// VFP/NEON register transfers (VMRS/VMSR/VMOV scalar transfer subset).
		if (coproc == 10 || coproc == 11) {
			// VMRS / VMSR FPSCR
			if (crn == 1 && crm == 0 && opc2 == 0 && opc1 == 7) {
				if (load) {  // VMRS
					if (rd == PC_INDEX) {
						cpsr = (cpsr & ~0xF0000000u) | (fpscr & 0xF0000000u);
					} else {
						write_reg(rd, fpscr);
					}
				} else {  // VMSR
					fpscr = get_reg_operand(rd);
				}
				if (!(load && rd == PC_INDEX)) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}

			// VMOV between ARM core register and single-precision VFP/NEON scalar S<n>.
			// Matches VMOVBRS-like register transfer subset.
			if (crm == 0 && opc2 == 0 && opc1 <= 1) {
				const u32 s_index = (((inst >> 16) & 0xF) << 1) | ((inst >> 7) & 1u);
				if (s_index < 32) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}

			// VMOV between ARM core register and D<n>[index] lane (32-bit lane transfer subset).
			// This provides a useful NEON scalar transfer path without full vector ALU implementation.
			if (crm == 0 && opc2 == 1) {
				const u32 d = ((inst >> 16) & 0xF) | (((inst >> 7) & 1u) << 4);
				const u32 lane = (inst >> 21) & 1u;
				const u32 s_index = d * 2 + lane;
				if (s_index < 32) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}
		}
	}

	// CLREX
	if ((inst & 0x0FFFFFF0u) == 0x057FF01Fu) {
		clear_exclusive();
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// ARM architectural hints / barriers in SYSTEM space.
	// NOP/YIELD/WFE/WFI/SEV and DMB/DSB/ISB are treated as synchronization no-ops in this interpreter path.
	if ((inst & 0x0FFFFFF0u) == 0x0320F000u || (inst & 0x0FFFFFF0u) == 0x0320F010u || (inst & 0x0FFFFFF0u) == 0x0320F020u ||
		(inst & 0x0FFFFFF0u) == 0x0320F030u || (inst & 0x0FFFFFF0u) == 0x0320F040u || (inst & 0x0FFFFFF0u) == 0x057FF040u ||
		(inst & 0x0FFFFFF0u) == 0x057FF050u || (inst & 0x0FFFFFF0u) == 0x057FF060u) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// VFP single-precision VLDR/VSTR (memory transfer subset).
	// Encoding form: cond 1101 U D 0/1 Rn Vd 101X imm8
	if ((inst & 0x0F000E00u) == 0x0D000A00u) {
		const bool load = (inst & (1u << 20)) != 0;
		const bool single = (inst & (1u << 8)) == 0;
		if (single) {
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 imm32 = (inst & 0xFFu) << 2;
			const bool add = (inst & (1u << 23)) != 0;
			if (vd < 32) {
				const u32 base = (rn == PC_INDEX) ? (((old_pc + 8) & ~3u)) : get_reg_operand(rn);
				const u32 addr = add ? (base + imm32) : (base - imm32);
				if (load) {
					extRegs[vd] = mem.read32(addr);
				} else {
					mem.write32(addr, extRegs[vd]);
					clear_exclusive();
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2;
			}
		} else {
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 imm32 = (inst & 0xFFu) << 2;
			const bool add = (inst & (1u << 23)) != 0;
			if (vd < 32) {
				const u32 base = (rn == PC_INDEX) ? (((old_pc + 8) & ~3u)) : get_reg_operand(rn);
				const u32 addr = add ? (base + imm32) : (base - imm32);
				if (load) {
					extRegs[vd * 2] = mem.read32(addr);
					extRegs[vd * 2 + 1] = mem.read32(addr + 4);
				} else {
					mem.write32(addr, extRegs[vd * 2]);
					mem.write32(addr + 4, extRegs[vd * 2 + 1]);
					clear_exclusive();
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 3;
			}
		}
	}

	// VFP single-precision VSTM/VLDM (multiple transfer subset, includes VPUSH/VPOP forms).
	// Encoding form: cond 110P U D W L Rn Vd 101X imm8
	if ((inst & 0x0F000E00u) == 0x0C000A00u) {
		const bool load = (inst & (1u << 20)) != 0;
		const bool single = (inst & (1u << 8)) == 0;
		if (single) {
			const bool add = (inst & (1u << 23)) != 0;
			const bool write_back = (inst & (1u << 21)) != 0;
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 regs = inst & 0xFFu;
			const u32 imm32 = regs << 2;

			if (vd + regs <= 32) {
				u32 address = (rn == PC_INDEX) ? (old_pc + 8) : gprs[rn];
				if (!add) {
					address -= imm32;
				}

				for (u32 i = 0; i < regs; i++) {
					if (load) {
						extRegs[vd + i] = mem.read32(address);
					} else {
						mem.write32(address, extRegs[vd + i]);
						clear_exclusive();
					}
					address += 4;
				}

				if (write_back) {
					gprs[rn] = add ? (gprs[rn] + imm32) : (gprs[rn] - imm32);
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2 + regs;
			}
		} else {
			const bool add = (inst & (1u << 23)) != 0;
			const bool write_back = (inst & (1u << 21)) != 0;
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 regs = (inst & 0xFEu) >> 1;
			const u32 imm32 = (inst & 0xFFu) << 2;

			if (vd + regs <= 32) {
				u32 address = (rn == PC_INDEX) ? (old_pc + 8) : gprs[rn];
				if (!add) {
					address -= imm32;
				}

				for (u32 i = 0; i < regs; i++) {
					if (load) {
						extRegs[(vd + i) * 2] = mem.read32(address);
						extRegs[(vd + i) * 2 + 1] = mem.read32(address + 4);
					} else {
						mem.write32(address, extRegs[(vd + i) * 2]);
						mem.write32(address + 4, extRegs[(vd + i) * 2 + 1]);
						clear_exclusive();
					}
					address += 8;
				}

				if (write_back) {
					gprs[rn] = add ? (gprs[rn] + imm32) : (gprs[rn] - imm32);
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2 + regs * 2;
			}
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

		if ((inst & 0xF0u) == 0xD0u && (rd & 1u) == 0) {  // STRD/LDRD
			if (load) {
				write_reg(rd, mem.read32(effective));
				write_reg(rd + 1, mem.read32(effective + 4));
			} else {
				mem.write32(effective, gprs[rd]);
				mem.write32(effective + 4, gprs[rd + 1]);
				clear_exclusive();
			}
		} else if (!load && sh == 0x1) {  // STRH
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
