#pragma once

#include "../../../include/helpers.hpp"
#include "../../../include/arm_defs.hpp"
#include <utility>
#include <string_view>

[[nodiscard]] constexpr u32 ror32(u32 value, u32 shift) {
	shift &= 31;
	if (shift == 0) return value;
	return (value >> shift) | (value << ((32 - shift) & 31));
}

[[nodiscard]] constexpr u32 signExtend(u32 value, u32 bits) {
	const u32 shift = 32 - bits;
	return static_cast<u32>(static_cast<s32>(value << shift) >> shift);
}

class VfpCoreExecutor {
public:
	static constexpr u32 kAzaharFPSID = 0x410120B4u;
	static constexpr u32 kAzaharMVFR0 = 0x11111111u;
	static constexpr u32 kAzaharMVFR1 = 0x00000000u;
	static constexpr u32 kAzaharFPINSTReset = 0xEE000A00u;

	static std::pair<u32, u32> VectorShape(u32 fpscr, u32 reg, bool is_double_precision);
	static bool IsSubnormalF32(u32 bits);
	static bool IsSubnormalF64(u64 bits);
	static void ApplyExceptionModel(u32& fpscr, u32& vfpFPEXC, u32& vfpFPINST, u32& vfpFPINST2,
									int host_exceptions, u32 fault_inst, int host_vfp_idc);
	static void ResetSystemRegs(u32& fpscr, u32& fpexc, u32& fpinst, u32& fpinst2,
								u32& fpsid, u32& mvfr0, u32& mvfr1);
};


class Coproc10Decoder {
public:
	enum class InstructionClass : u32 { Cdp, LdcStc, StructuredLdcStc, McrMrc, McrrMrrc, Other };
	enum class Class : u32 { DataProc, MemTransfer, StructuredMemTransfer, RegTransfer, Unknown };

	static InstructionClass DecodeInstructionClass(u32 opcode);
	static Class DecodeClass(u32 opcode);

	enum class StructuredKind : u32 { None, VLD1, VLD2, VLD3, VLD4, VST1, VST2, VST3, VST4 };
	struct MemDecode {
		bool load, add, write_back, interleaved;
		u32 interleave_factor, rn, vd, transfer_words, base, address, structure_regs;
		StructuredKind structured_kind;
	};

	template <typename GetRegFn>
	static MemDecode DecodeMem(u32 opcode, u32 old_pc, GetRegFn&& get_reg_operand) {
		MemDecode dec{};
		dec.load = (opcode & (1u << 20)) != 0;
		dec.add = (opcode & (1u << 23)) != 0;
		dec.write_back = (opcode & (1u << 21)) != 0;
		dec.rn = (opcode >> 16) & 0xFu;
		dec.vd = ((opcode >> 12) & 0xFu) | (((opcode >> 22) & 1u) << 4);
		const u32 words = opcode & 0xFFu;
		dec.transfer_words = words == 0 ? 1u : words;
		dec.interleaved = (opcode & (1u << 5)) != 0;
		dec.interleave_factor = dec.interleaved ? (((opcode & (1u << 6)) != 0) ? 4u : 2u) : 1u;
		dec.structure_regs = dec.interleave_factor;
		dec.structured_kind = StructuredKind::None;
		if (dec.interleaved) {
			if (dec.interleave_factor == 2) dec.structured_kind = dec.load ? StructuredKind::VLD2 : StructuredKind::VST2;
			else if (dec.interleave_factor == 4) dec.structured_kind = dec.load ? StructuredKind::VLD4 : StructuredKind::VST4;
			if (dec.transfer_words > dec.interleave_factor) {
				if (dec.interleave_factor == 2) { dec.structured_kind = dec.load ? StructuredKind::VLD3 : StructuredKind::VST3; dec.structure_regs = 3; }
				else { dec.structured_kind = dec.load ? StructuredKind::VLD1 : StructuredKind::VST1; dec.structure_regs = 1; }
			}
		}
		dec.base = (dec.rn == 15) ? ((old_pc + 8) & ~3u) : get_reg_operand(dec.rn);
		dec.address = dec.add ? dec.base : (dec.base - dec.transfer_words * 4);
		return dec;
	}
};

enum class OpcodeID : u32 {
    ADC, ADD, AND, B, BFC, BFI, BIC, BKPT, BL, BLX, BLX_IMM, BX, CLZ, CMN, CMP,
    CPS, DMB, DSB, EOR, ISB, LDC, LDM, LDR, LDRB, LDREX, LDREXB, LDREXD, LDREXH,
    LDRH, LDRSB, LDRSH, MLA, MOV, MOVT, MOVW, MRS, MSR, MSR_IMM, MUL, MVN, ORR,
    PKHBT, PKHTB, PLD, PLDW, QADD, QDADD, QDSUB, QSUB, REV, REV16, REVSH, RSB,
    RSC, SADD16, SADD8, SADDSUBX, SBC, SBFX, SEL, SMLAXY, SMLAL, SMULL, SSAT,
    SSAT16, SSUB16, SSUB8, SSUBADDX, STC, STM, STR, STRB, STREX, STREXB, STREXD,
    STREXH, STRH, SUB, SWP, SWPB, SXTAB, SXTAB16, SXTAH, SXTB, SXTB16, SXTH,
    TEQ, TST, UBFX, UMAAL, UMLAL, UMULL, USAT, USAT16, UXTAB, UXTAB16, UXTAH,
    UXTB, UXTB16, UXTH, VABS_F32, VABS_F64, VADD_F32, VADD_F64, VCMP_F32,
    VCMP_F64, VCVT_F32_F64, VCVT_F64_F32, VCVT_S32_F32, VCVT_U32_F32, VDIV_F32,
    VDIV_F64, VLDM, VLDR, VMLA_F32, VMLA_F64, VMLS_F32, VMLS_F64, VMOV_I, VMOV_R,
    VNEG_F32, VNEG_F64, VNMLA_F32, VNMLA_F64, VNMLS_F32, VNMLS_F64, VSQRT_F32,
    VSQRT_F64, VSTM, VSTR, WFE, WFI, YIELD, SEV, CLREX, SVC, RFE, SRS, NOP,
    UNDEFINED
};

struct InstructionDecoding {
	const char* name;
	u32 opcode_mask;
	u32 opcode_value;
	int version;
    OpcodeID id;
};

const InstructionDecoding* DecodeArmInstruction(u32 inst);

constexpr u32 PC_INDEX = 15;
constexpr u32 LR_INDEX = 14;

#define RN (gprs[(inst >> 16) & 0xF])
#define RM (gprs[inst & 0xF])
#define RD (gprs[(inst >> 12) & 0xF])
#define RS (gprs[(inst >> 8) & 0xF])
#define SHIFTER_OPERAND (getShifterOperand(inst, shifter_carry))
#define BITS(s, a, b) ((static_cast<u32>(s) << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) (((s) >> (n)) & 1)
#define UPDATE_NFLAG(rd) setNZFlags(rd)
#define UPDATE_ZFLAG(rd)
#define UPDATE_CFLAG_WITH_SC CFlag = shifter_carry
#define LOAD_NZCVT loadNZCVT()
#define SAVE_NZCVT saveNZCVT()
#define POS(i) ((~(i)) >> 31)
#define NEG(i) ((i) >> 31)
#define INC_PC_STUB (void)0
#define LINK_RTN_ADDR gprs[14] = old_pc + 4
#define CHECK_READ_REG15(cpu, reg) (((reg) == 15) ? (old_pc + 8) : gprs[reg])
#define CHECK_READ_REG15_WA(cpu, reg) (((reg) == 15) ? (old_pc + 8) : gprs[reg])

inline u32 AddWithCarry(u32 x, u32 y, u32 c_in, bool* c_out, bool* v_out) {
    u64 result = static_cast<u64>(x) + static_cast<u64>(y) + c_in;
    if (c_out) *c_out = (result >> 32) != 0;
    u32 res32 = static_cast<u32>(result);
    if (v_out) *v_out = ((~(x ^ y) & (x ^ res32)) & 0x80000000u) != 0;
    return res32;
}

inline u32 SubWithBorrow(u32 x, u32 y, u32 c_in, bool* c_out, bool* v_out) {
    u64 result = static_cast<u64>(x) - static_cast<u64>(y) - (1 - c_in);
    if (c_out) *c_out = (result >> 32) == 0;
    u32 res32 = static_cast<u32>(result);
    if (v_out) *v_out = (((x ^ y) & (x ^ res32)) & 0x80000000u) != 0;
    return res32;
}

inline bool AddOverflow(u32 a, u32 b, u32 result) {
    return ((NEG(a) && NEG(b) && POS(result)) || (POS(a) && POS(b) && NEG(result)));
}

inline bool SubOverflow(u32 a, u32 b, u32 result) {
    return ((NEG(a) && POS(b) && POS(result)) || (POS(a) && NEG(b) && NEG(result)));
}

inline u8 ARMul_SignedSaturatedAdd8(u8 a, u8 b) {
    s32 res = (s8)a + (s8)b;
    if (res > 127) return 127;
    if (res < -128) return (u8)128;
    return (u8)res;
}
inline u8 ARMul_SignedSaturatedSub8(u8 a, u8 b) {
    s32 res = (s8)a - (s8)b;
    if (res > 127) return 127;
    if (res < -128) return (u8)128;
    return (u8)res;
}
inline u16 ARMul_SignedSaturatedAdd16(u16 a, u16 b) {
    s32 res = (s16)a + (s16)b;
    if (res > 32767) return 32767;
    if (res < -32768) return (u16)32768;
    return (u16)res;
}
inline u16 ARMul_SignedSaturatedSub16(u16 a, u16 b) {
    s32 res = (s16)a - (s16)b;
    if (res > 32767) return 32767;
    if (res < -32768) return (u16)32768;
    return (u16)res;
}
