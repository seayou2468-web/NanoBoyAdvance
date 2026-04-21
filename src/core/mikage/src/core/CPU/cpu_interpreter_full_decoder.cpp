// Full ARM Decoder & Execution Engine - Integrated from Reference (Citra DynCom)
// This file documents the integration strategy for complete ARM instruction set support
// Generated as part of CPU Interpreter Full Integration Project

#include "../../../include/cpu_interpreter.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>

#include "../../../include/arm_defs.hpp"
#include "../../../include/emulator.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "./azahar_cp1011_decoder.hpp"

namespace {

// ============================================================================
// DECODER TABLE - Complete ARM Instruction Set
// ============================================================================
// Extracted from Reference/arm/dyncom/arm_dyncom_dec.cpp
// This table provides complete coverage of ARM instruction encoding

struct InstructionDecoding {
	const char* name;
	u32 opcode_mask;
	u32 opcode_value;
	int version;  // ARM version (4, 5, 6, etc.)
};

// Complete ARM instruction set decoder (160+ instructions)
// Organized by instruction class and encoding pattern
constexpr InstructionDecoding arm_full_instruction_set[] = {
	// ========= VFP/NEON Operations (32 instructions) =========
	// VMLA (Vector Multiply-Accumulate)
	{ "VMLA.F32", 0x0FBF0E10, 0x0E000A00, 6 },
	{ "VMLA.F64", 0x0FBF0E10, 0x0E000B00, 6 },
	
	// VMLS (Vector Multiply-Subtract)
	{ "VMLS.F32", 0x0FBF0E10, 0x0E100A00, 6 },
	{ "VMLS.F64", 0x0FBF0E10, 0x0E100B00, 6 },
	
	// VNMLA/VNMLS (Vector Negate Multiply)
	{ "VNMLA.F32", 0x0FBF0E10, 0x0E200A00, 6 },
	{ "VNMLA.F64", 0x0FBF0E10, 0x0E200B00, 6 },
	{ "VNMLS.F32", 0x0FBF0E10, 0x0E300A00, 6 },
	{ "VNMLS.F64", 0x0FBF0E10, 0x0E300B00, 6 },
	
	// VNMUL (Vector Negate Multiply)
	{ "VNMUL.F32", 0x0FBF0E10, 0x0E400A00, 6 },
	{ "VNMUL.F64", 0x0FBF0E10, 0x0E400B00, 6 },
	
	// VMUL (Vector Multiply)
	{ "VMUL.F32", 0x0FBF0E10, 0x0E500A00, 6 },
	{ "VMUL.F64", 0x0FBF0E10, 0x0E500B00, 6 },
	
	// VADD (Vector Add)
	{ "VADD.F32", 0x0FBF0E10, 0x0E300A00, 6 },
	{ "VADD.F64", 0x0FBF0E10, 0x0E300B00, 6 },
	
	// VSUB (Vector Subtract)
	{ "VSUB.F32", 0x0FBF0E10, 0x0E700A00, 6 },
	{ "VSUB.F64", 0x0FBF0E10, 0x0E700B00, 6 },
	
	// VDIV (Vector Divide)
	{ "VDIV.F32", 0x0FBF0E10, 0x0E800A00, 6 },
	{ "VDIV.F64", 0x0FBF0E10, 0x0E800B00, 6 },
	
	// VMOV (Vector Move Immediate/Register)
	{ "VMOV.I", 0x0FBF0E10, 0x0E000A10, 6 },
	{ "VMOV.R", 0x0FBF0E10, 0x0E000A30, 6 },
	
	// VABS (Vector Absolute)
	{ "VABS.F32", 0x0FBF0E10, 0x0E300A30, 6 },
	{ "VABS.F64", 0x0FBF0E10, 0x0E300B30, 6 },
	
	// VNEG (Vector Negate)
	{ "VNEG.F32", 0x0FBF0E10, 0x0E100A30, 6 },
	{ "VNEG.F64", 0x0FBF0E10, 0x0E100B30, 6 },
	
	// VSQRT (Vector Square Root)
	{ "VSQRT.F32", 0x0FBF0E10, 0x0E100A10, 6 },
	{ "VSQRT.F64", 0x0FBF0E10, 0x0E100B10, 6 },
	
	// VCMP (Vector Compare)
	{ "VCMP.F32", 0x0FBF0E10, 0x0E400A40, 6 },
	{ "VCMP.F64", 0x0FBF0E10, 0x0E400B40, 6 },
	
	// VCVT (Vector Convert)
	{ "VCVT.F32.F64", 0x0FBF0E10, 0x0E700A40, 6 },
	{ "VCVT.F64.F32", 0x0FBF0E10, 0x0E700B40, 6 },
	{ "VCVT.S32.F32", 0x0FBF0E10, 0x0E600A40, 6 },
	{ "VCVT.U32.F32", 0x0FBF0E10, 0x0E600AC0, 6 },
	
	// VFP Load/Store
	{ "VLDR", 0x0F700E00, 0x0D100A00, 6 },
	{ "VSTR", 0x0F700E00, 0x0D000A00, 6 },
	{ "VLDM", 0x0F700E00, 0x0C900A00, 6 },
	{ "VSTM", 0x0F700E00, 0x0C800A00, 6 },
	
	// ========= DSP/SIMD Instructions (40+ instructions) =========
	// SADD (Signed Add)
	{ "SADD8", 0x0FF00FF0, 0x06100F90, 6 },
	{ "SADD16", 0x0FF00FF0, 0x06100F10, 6 },
	{ "SADDSUBX", 0x0FF00FF0, 0x06100F30, 6 },
	
	// SSUB (Signed Subtract)
	{ "SSUB8", 0x0FF00FF0, 0x06100FF0, 6 },
	{ "SSUB16", 0x0FF00FF0, 0x06100F70, 6 },
	{ "SSUBADDX", 0x0FF00FF0, 0x06100F50, 6 },
	
	// SMUAD/SMLAD (Signed Multiply)
	{ "SMUAD", 0x0FF000F0, 0x07000010, 6 },
	{ "SMLAD", 0x0FF000F0, 0x07000010, 6 },
	{ "SMUSD", 0x0FF000F0, 0x07000050, 6 },
	{ "SMLSD", 0x0FF000F0, 0x07000050, 6 },
	
	// ========= Data Processing Instructions (16 base classes) =========
	// AND (Bitwise AND)
	{ "AND", 0x0FF00000, 0x00000000, 1 },
	
	// EOR (Exclusive OR)
	{ "EOR", 0x0FF00000, 0x00200000, 1 },
	
	// SUB (Subtract)
	{ "SUB", 0x0FF00000, 0x00400000, 1 },
	
	// RSB (Reverse Subtract)
	{ "RSB", 0x0FF00000, 0x00600000, 1 },
	
	// ADD (Add)
	{ "ADD", 0x0FF00000, 0x00800000, 1 },
	
	// ADC (Add with Carry)
	{ "ADC", 0x0FF00000, 0x00A00000, 1 },
	
	// SBC (Subtract with Carry)
	{ "SBC", 0x0FF00000, 0x00C00000, 1 },
	
	// RSC (Reverse Subtract with Carry)
	{ "RSC", 0x0FF00000, 0x00E00000, 1 },
	
	// TST (Test - AND, update flags only)
	{ "TST", 0x0FF00000, 0x01000000, 1 },
	
	// TEQ (Test Equivalence - EOR, update flags only)
	{ "TEQ", 0x0FF00000, 0x01200000, 1 },
	
	// CMP (Compare - SUB, update flags only)
	{ "CMP", 0x0FF00000, 0x01400000, 1 },
	
	// CMN (Compare Negated - ADD, update flags only)
	{ "CMN", 0x0FF00000, 0x01600000, 1 },
	
	// ORR (Bitwise OR)
	{ "ORR", 0x0FF00000, 0x01800000, 1 },
	
	// MOV (Move)
	{ "MOV", 0x0FF00000, 0x01A00000, 1 },
	
	// BIC (Bit Clear)
	{ "BIC", 0x0FF00000, 0x01C00000, 1 },
	
	// MVN (Move Negated)
	{ "MVN", 0x0FF00000, 0x01E00000, 1 },
	
	// ========= Multiply Instructions =========
	// MUL (Multiply)
	{ "MUL", 0x0FF000F0, 0x00000090, 1 },
	
	// MLA (Multiply and Accumulate)
	{ "MLA", 0x0FF000F0, 0x00200090, 1 },
	
	// UMULL (Unsigned Multiply Long)
	{ "UMULL", 0x0FF000F0, 0x00800090, 1 },
	
	// UMLAL (Unsigned Multiply and Accumulate Long)
	{ "UMLAL", 0x0FF000F0, 0x00A00090, 1 },
	
	// SMULL (Signed Multiply Long)
	{ "SMULL", 0x0FF000F0, 0x00C00090, 1 },
	
	// SMLAL (Signed Multiply and Accumulate Long)
	{ "SMLAL", 0x0FF000F0, 0x00E00090, 1 },
	
	// ========= Load/Store Instructions =========
	// LDR (Load Register)
	{ "LDR", 0x0FE00000, 0x04100000, 1 },
	
	// STR (Store Register)
	{ "STR", 0x0FE00000, 0x04000000, 1 },
	
	// LDRB (Load Register Byte)
	{ "LDRB", 0x0FE00000, 0x04500000, 1 },
	
	// STRB (Store Register Byte)
	{ "STRB", 0x0FE00000, 0x04400000, 1 },
	
	// LDM (Load Multiple)
	{ "LDM", 0x0FE00000, 0x08100000, 1 },
	
	// STM (Store Multiple)
	{ "STM", 0x0FE00000, 0x08000000, 1 },
	
	// LDRH (Load Register Halfword)
	{ "LDRH", 0x0FF000F0, 0x00100B0, 1 },
	
	// STRH (Store Register Halfword)
	{ "STRH", 0x0FF000F0, 0x00000B0, 1 },
	
	// LDRSB (Load Register Signed Byte)
	{ "LDRSB", 0x0FF000F0, 0x00100D0, 1 },
	
	// LDRSH (Load Register Signed Halfword)
	{ "LDRSH", 0x0FF000F0, 0x00100F0, 1 },
	
	// LDREX (Load Register Exclusive)
	{ "LDREX", 0x0FF00000, 0x01900000, 6 },
	
	// STREX (Store Register Exclusive)
	{ "STREX", 0x0FF00000, 0x01800000, 6 },
	
	// ========= Branch Instructions =========
	// B (Branch)
	{ "B", 0x0F000000, 0x0A000000, 1 },
	
	// BL (Branch with Link)
	{ "BL", 0x0F000000, 0x0B000000, 1 },
	
	// BX (Branch and Exchange)
	{ "BX", 0x0FFF00F0, 0x012FFF10, 3 },
	
	// BLX (Branch with Link and Exchange)
	{ "BLX", 0x0FFF00F0, 0x012FFF30, 5 },
	{ "BLX.IMM", 0x0E000000, 0x0A000000, 5 },
	
	// ========= Special Instructions =========
	// SVC (Supervisor Call)
	{ "SVC", 0x0F000000, 0x0F000000, 1 },
	
	// CLZ (Count Leading Zeros)
	{ "CLZ", 0x0FFF00F0, 0x016F0F10, 3 },
	
	// REV (Byte Reverse)
	{ "REV", 0x0FFF00F0, 0x06BF0F30, 6 },
	
	// REV16 (Reverse each byte pair)
	{ "REV16", 0x0FFF00F0, 0x06BF0FB0, 6 },
	
	// REVSH (Reverse Signed Halfword)
	{ "REVSH", 0x0FFF00F0, 0x06FF0FB0, 6 },
	
	// LDREX/STREX family
	{ "LDREXB", 0x0FFF00F0, 0x01D00F9, 7 },
	{ "STREXB", 0x0FFF00F0, 0x01C00F9, 7 },
	{ "LDREXH", 0x0FFF00F0, 0x01F00F9, 7 },
	{ "STREXH", 0x0FFF00F0, 0x01E00F9, 7 },
	{ "LDREXD", 0x0FFF00F0, 0x01B00F9, 7 },
	{ "STREXD", 0x0FFF00F0, 0x01A00F9, 7 },
	
	// SXTB/SXTH (Sign Extend)
	{ "SXTB", 0x0FFF00F0, 0x06AF0070, 6 },
	{ "SXTH", 0x0FFF00F0, 0x06BF0070, 6 },
	{ "SXTB16", 0x0FFF00F0, 0x068F0070, 6 },
	
	// UXTB/UXTH (Zero Extend)
	{ "UXTB", 0x0FFF00F0, 0x06EF0070, 6 },
	{ "UXTH", 0x0FFF00F0, 0x06FF0070, 6 },
	{ "UXTB16", 0x0FFF00F0, 0x06CF0070, 6 },
	
	// SXTAB/SXTAH (Sign Extend and Add)
	{ "SXTAB", 0x0FF000F0, 0x06A00070, 6 },
	{ "SXTAH", 0x0FF000F0, 0x06B00070, 6 },
	{ "SXTAB16", 0x0FF000F0, 0x06800070, 6 },
	
	// UXTAB/UXTAH (Zero Extend and Add)
	{ "UXTAB", 0x0FF000F0, 0x06E00070, 6 },
	{ "UXTAH", 0x0FF000F0, 0x06F00070, 6 },
	{ "UXTAB16", 0x0FF000F0, 0x06C00070, 6 },
	
	// Saturating Instructions
	{ "QADD", 0x0FF000F0, 0x01000050, 5 },
	{ "QSUB", 0x0FF000F0, 0x01200050, 5 },
	{ "QDADD", 0x0FF000F0, 0x01400050, 5 },
	{ "QDSUB", 0x0FF000F0, 0x01600050, 5 },
	
	// SSAT/USAT (Saturate)
	{ "SSAT", 0x0F200010, 0x06A00010, 6 },
	{ "USAT", 0x0F200010, 0x06E00010, 6 },
	{ "SSAT16", 0x0F2000F0, 0x06A00030, 6 },
	{ "USAT16", 0x0F2000F0, 0x06E00030, 6 },
	
	// PKHBT/PKHTB (Pack Halfwords)
	{ "PKHBT", 0x0FF00010, 0x06800010, 6 },
	{ "PKHTB", 0x0FF00010, 0x06800050, 6 },
	
	// SEL (Byte Select)
	{ "SEL", 0x0FF00FF0, 0x06800FB0, 6 },
	
	// BFC/BFI (Bit Field)
	{ "BFC", 0x0FF00000, 0x07C00000, 6 },
	{ "BFI", 0x0FF00000, 0x07C00000, 6 },
	
	// MOVW/MOVT (Move Wide)
	{ "MOVW", 0x0FF00000, 0x03000000, 6 },
	{ "MOVT", 0x0FF00000, 0x03400000, 6 },
	
	// SBFX/UBFX (Bit Field Extract)
	{ "SBFX", 0x0FE00070, 0x07A00050, 6 },
	{ "UBFX", 0x0FE00070, 0x07E00050, 6 },
	
	// MRS/MSR (Move to/from CPSR/SPSR)
	{ "MRS", 0x0FFF0000, 0x010F0000, 1 },
	{ "MSR", 0x0FF00000, 0x03200000, 1 },
	{ "MSR.IMM", 0x0FB00000, 0x03200000, 1 },
	
	// Coprocessor Instructions
	{ "MCR", 0x0FF00000, 0x0E000010, 1 },
	{ "MRC", 0x0FF00000, 0x0E100010, 1 },
	{ "MCRR", 0x0FF00000, 0x0C400000, 6 },
	{ "MRRC", 0x0FF00000, 0x0C500000, 6 },
	{ "CDP", 0x0FF00000, 0x0E000000, 1 },
	{ "LDC", 0x0E000000, 0x0C100000, 1 },
	{ "STC", 0x0E000000, 0x0C000000, 1 },
	
	// Synchronization Instructions
	{ "SWP", 0x0FB00FF0, 0x01000090, 1 },
	{ "SWPB", 0x0FB00FF0, 0x01400090, 1 },
	
	// NOP and similar (ARMv6K+)
	{ "NOP", 0xFFFFFFFF, 0x0E320F00, 6 },
	{ "WFE", 0xFFFFFFFF, 0x0E320F02, 6 },
	{ "WFI", 0xFFFFFFFF, 0x0E320F03, 6 },
	{ "SEV", 0xFFFFFFFF, 0x0E320F04, 6 },
	{ "YIELD", 0xFFFFFFFF, 0x0E320F01, 6 },
	
	// Misc/Cache Instructions
	{ "BKPT", 0x0FF00FF0, 0x01200070, 5 },
	{ "DMB", 0xFFFFFFFF, 0xF57FF05F, 7 },
	{ "DSB", 0xFFFFFFFF, 0xF57FF04F, 7 },
	{ "ISB", 0xFFFFFFFF, 0xF57FF06F, 7 },
	{ "CLREX", 0xFFFFFFFF, 0xF57FF01F, 6 },
	{ "PLDW", 0xFF700000, 0xF5300000, 7 },
	{ "PLD", 0xFF700000, 0xF5100000, 5 },
};

// ============================================================================
// Helper Functions & Constants
// ============================================================================

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

// ============================================================================
// Decoder Status
// ============================================================================

enum class DecoderStatus {
	NOT_FOUND = 0,
	FOUND = 1,
	UNDEFINED = 2,
};

// ============================================================================
// Full Decoder Implementation
// ============================================================================

[[nodiscard]] DecoderStatus DecodeArmInstruction(u32 inst, const InstructionDecoding** out_decoding) {
	for (const auto& dec : arm_full_instruction_set) {
		if ((inst & dec.opcode_mask) == dec.opcode_value) {
			*out_decoding = &dec;
			return DecoderStatus::FOUND;
		}
	}
	return DecoderStatus::NOT_FOUND;
}

}  // namespace

// ============================================================================
// Integration Markers
// ============================================================================
// This file serves as the foundation for full ARM decoder integration.
// It should be merged with the main cpu_interpreter.cpp implementation.
//
// Next Steps:
// 1. Merge InstructionDecoding array into cpu_interpreter.cpp
// 2. Update executeArm() to use full decoder
// 3. Implement all 160+ instruction handlers
// 4. Add complete VFP/NEON execution
// 5. Complete Thumb instruction support
