#include "../../../include/cpu_interpreter.hpp"
#include "./cpu_interpreter_internal.hpp"
#include <cfenv>
#include <cmath>

std::pair<u32, u32> VfpCoreExecutor::VectorShape(u32 fpscr, u32 reg, bool is_double_precision) {
	const u32 fpscr_len = (fpscr >> 16) & 0x7u;
	const u32 bank_mask = is_double_precision ? ~3u : ~7u;
	const u32 vec_elements = ((reg & bank_mask) == 0 || fpscr_len == 0) ? 1u : (fpscr_len + 1u);
	const u32 vec_stride = ((fpscr >> 20) & 0x3u) == 0x3u ? 2u : 1u;
	return {vec_elements, vec_stride};
}

bool VfpCoreExecutor::IsSubnormalF32(u32 bits) {
	return (bits & 0x7F800000u) == 0 && (bits & 0x007FFFFFu) != 0;
}

bool VfpCoreExecutor::IsSubnormalF64(u64 bits) {
	return (bits & 0x7FF0000000000000ull) == 0 && (bits & 0x000FFFFFFFFFFFFFull) != 0;
}

void VfpCoreExecutor::ApplyExceptionModel(u32& fpscr, u32& vfpFPEXC, u32& vfpFPINST, u32& vfpFPINST2,
									int host_exceptions, u32 fault_inst, int host_vfp_idc) {
	const auto raise = [&](u32 cumulative_bit, u32 enable_bit, u32 fpexc_trap_bit) {
		fpscr |= cumulative_bit;
		if ((fpscr & enable_bit) && (vfpFPEXC & (1u << 30))) {
			const bool first_fault = (vfpFPEXC & (1u << 31)) == 0;
			vfpFPEXC |= (1u << 31) | (1u << 29) | fpexc_trap_bit;
			if (first_fault) {
				vfpFPINST = fault_inst;
				vfpFPINST2 = 0;
			}
		}
	};

	if (host_exceptions & FE_INVALID) raise(1u << 0, 1u << 8, 1u << 0);
	if (host_exceptions & FE_DIVBYZERO) raise(1u << 1, 1u << 9, 1u << 1);
	if (host_exceptions & FE_OVERFLOW) raise(1u << 2, 1u << 10, 1u << 2);
	if (host_exceptions & FE_UNDERFLOW) raise(1u << 3, 1u << 11, 1u << 3);
	if (host_exceptions & FE_INEXACT) raise(1u << 4, 1u << 12, 1u << 4);
	if (host_exceptions & host_vfp_idc) raise(1u << 7, 1u << 15, 1u << 7);
}

void VfpCoreExecutor::ResetSystemRegs(u32& fpscr, u32& fpexc, u32& fpinst, u32& fpinst2,
								u32& fpsid, u32& mvfr0, u32& mvfr1) {
	fpscr = FPSCR::MainThreadDefault;
	fpexc = 0;
	fpinst = kAzaharFPINSTReset;
	fpinst2 = 0;
	fpsid = kAzaharFPSID;
	mvfr0 = kAzaharMVFR0;
	mvfr1 = kAzaharMVFR1;
}

Coproc10Decoder::InstructionClass Coproc10Decoder::DecodeInstructionClass(u32 opcode) {
	if ((opcode & 0x0F0000F0u) == 0x0C400000u || (opcode & 0x0F0000F0u) == 0x0C500000u) {
		return InstructionClass::McrrMrrc;
	}
	if ((opcode & 0x0F000010u) == 0x0E000010u) {
		return InstructionClass::McrMrc;
	}
	if ((opcode & 0x0F000000u) == 0x0C000000u || (opcode & 0x0F000000u) == 0x0D000000u) {
		return (opcode & (1u << 5)) ? InstructionClass::StructuredLdcStc : InstructionClass::LdcStc;
	}
	if ((opcode & 0x0F000010u) == 0x0E000000u) {
		return InstructionClass::Cdp;
	}
	return InstructionClass::Other;
}

Coproc10Decoder::Class Coproc10Decoder::DecodeClass(u32 opcode) {
	if ((opcode & 0x0F000010u) == 0x0E000000u) return Class::DataProc;
	if ((opcode & 0x0F000000u) == 0x0C000000u || (opcode & 0x0F000000u) == 0x0D000000u) {
		if ((opcode & (1u << 5)) != 0) {
			return Class::StructuredMemTransfer;
		}
		return Class::MemTransfer;
	}
	if ((opcode & 0x0F000010u) == 0x0E000010u || (opcode & 0x0F0000F0u) == 0x0C400000u ||
		(opcode & 0x0F0000F0u) == 0x0C500000u) {
		return Class::RegTransfer;
	}
	return Class::Unknown;
}

#include "cpu_interpreter_table.inc"

const InstructionDecoding* DecodeArmInstruction(u32 inst) {
	for (const auto& dec : arm_full_instruction_set) {
		if ((inst & dec.opcode_mask) == dec.opcode_value) {
			return &dec;
		}
	}
	return nullptr;
}
