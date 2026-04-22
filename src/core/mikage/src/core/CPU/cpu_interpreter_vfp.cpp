#include "../../../include/cpu_interpreter.hpp"
#include "./cpu_interpreter_internal.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
constexpr int kHostVfpIdc = 1 << 20;

inline u32 GetSingleIndexFromD(u32 d, u32 lane) {
	return d * 2 + lane;
}

inline float BitsToF32(u32 bits) {
	return std::bit_cast<float>(bits);
}

inline u32 F32ToBits(float value) {
	return std::bit_cast<u32>(value);
}

inline double BitsToF64(u64 bits) {
	return std::bit_cast<double>(bits);
}

inline u64 F64ToBits(double value) {
	return std::bit_cast<u64>(value);
}
}

u32 CPU::executeVfp(u32 inst) {
	const u32 old_pc = gprs[PC_INDEX];
	const u32 cond = inst >> 28;
	if (cond != 0xE && !conditionPassed(cond)) return 1;

	const InstructionDecoding* decoding = DecodeArmInstruction(inst);
	if (!decoding) {
		return executeCoproc(inst);
	}

	const u32 sd = (((inst >> 12) & 0xFu) << 1) | ((inst >> 22) & 1u);
	const u32 sn = (((inst >> 16) & 0xFu) << 1) | ((inst >> 7) & 1u);
	const u32 sm = ((inst & 0xFu) << 1) | ((inst >> 5) & 1u);
	const u32 dd = ((inst >> 12) & 0xFu) | (((inst >> 22) & 1u) << 4);
	const u32 dn = ((inst >> 16) & 0xFu) | (((inst >> 7) & 1u) << 4);
	const u32 dm = (inst & 0xFu) | (((inst >> 5) & 1u) << 4);

	const auto apply_exceptions = [&](int host_exceptions, int idc_flag) {
		VfpCoreExecutor::ApplyExceptionModel(fpscr, vfpFPEXC, vfpFPINST, vfpFPINST2, host_exceptions,
											 inst, idc_flag);
	};

	const auto run_f32_binary = [&](auto&& fn) -> bool {
		if (sd >= 32 || sn >= 32 || sm >= 32) return false;
		const auto [vec_elements, vec_stride] = VfpCoreExecutor::VectorShape(fpscr, sd, false);
		for (u32 i = 0; i < vec_elements; i++) {
			const u32 dst = (sd + i * vec_stride) & 31u;
			const u32 lhs = (sn + i * vec_stride) & 31u;
			const u32 rhs = (sm + i * vec_stride) & 31u;
			const int idc_flag = (VfpCoreExecutor::IsSubnormalF32(extRegs[lhs]) ||
								  VfpCoreExecutor::IsSubnormalF32(extRegs[rhs]))
									 ? kHostVfpIdc
									 : 0;
			std::feclearexcept(FE_ALL_EXCEPT);
			const float out = fn(BitsToF32(extRegs[lhs]), BitsToF32(extRegs[rhs]));
			extRegs[dst] = F32ToBits(out);
			apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT) | idc_flag, kHostVfpIdc);
		}
		return true;
	};

	const auto run_f64_binary = [&](auto&& fn) -> bool {
		if (dd >= 32 || dn >= 32 || dm >= 32) return false;
		const auto [vec_elements, vec_stride] = VfpCoreExecutor::VectorShape(fpscr, dd, true);
		for (u32 i = 0; i < vec_elements; i++) {
			const u32 dst = (dd + i * vec_stride) & 31u;
			const u32 lhs = (dn + i * vec_stride) & 31u;
			const u32 rhs = (dm + i * vec_stride) & 31u;
			const u64 lhs_bits = (static_cast<u64>(extRegs[GetSingleIndexFromD(lhs, 1)]) << 32) |
								 extRegs[GetSingleIndexFromD(lhs, 0)];
			const u64 rhs_bits = (static_cast<u64>(extRegs[GetSingleIndexFromD(rhs, 1)]) << 32) |
								 extRegs[GetSingleIndexFromD(rhs, 0)];
			const int idc_flag =
				(VfpCoreExecutor::IsSubnormalF64(lhs_bits) || VfpCoreExecutor::IsSubnormalF64(rhs_bits))
					? kHostVfpIdc
					: 0;
			std::feclearexcept(FE_ALL_EXCEPT);
			const double out = fn(BitsToF64(lhs_bits), BitsToF64(rhs_bits));
			const u64 out_bits = F64ToBits(out);
			extRegs[GetSingleIndexFromD(dst, 0)] = static_cast<u32>(out_bits);
			extRegs[GetSingleIndexFromD(dst, 1)] = static_cast<u32>(out_bits >> 32);
			apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT) | idc_flag, kHostVfpIdc);
		}
		return true;
	};

	bool handled = true;
	switch (decoding->id) {
		case OpcodeID::VADD_F32: handled = run_f32_binary([](float a, float b) { return a + b; }); break;
		case OpcodeID::VSUB_F32: handled = run_f32_binary([](float a, float b) { return a - b; }); break;
		case OpcodeID::VMLA_F32: handled = run_f32_binary([](float a, float b) { return a * b; }); break;
		case OpcodeID::VMLS_F32: handled = run_f32_binary([](float a, float b) { return a * (-b); }); break;
		case OpcodeID::VDIV_F32: handled = run_f32_binary([](float a, float b) { return a / b; }); break;
		case OpcodeID::VADD_F64: handled = run_f64_binary([](double a, double b) { return a + b; }); break;
		case OpcodeID::VSUB_F64: handled = run_f64_binary([](double a, double b) { return a - b; }); break;
		case OpcodeID::VMLA_F64: handled = run_f64_binary([](double a, double b) { return a * b; }); break;
		case OpcodeID::VMLS_F64: handled = run_f64_binary([](double a, double b) { return a * (-b); }); break;
		case OpcodeID::VDIV_F64: handled = run_f64_binary([](double a, double b) { return a / b; }); break;
		case OpcodeID::VABS_F32:
			if (sd < 32 && sm < 32) extRegs[sd] = F32ToBits(std::fabs(BitsToF32(extRegs[sm])));
			else handled = false;
			break;
		case OpcodeID::VNEG_F32:
			if (sd < 32 && sm < 32) extRegs[sd] = F32ToBits(-BitsToF32(extRegs[sm]));
			else handled = false;
			break;
		case OpcodeID::VSQRT_F32:
			if (sd < 32 && sm < 32) {
				std::feclearexcept(FE_ALL_EXCEPT);
				const int idc_flag = VfpCoreExecutor::IsSubnormalF32(extRegs[sm]) ? kHostVfpIdc : 0;
				extRegs[sd] = F32ToBits(std::sqrt(BitsToF32(extRegs[sm])));
				apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT) | idc_flag, kHostVfpIdc);
			} else handled = false;
			break;
		case OpcodeID::VCMP_F32:
			if (sd < 32 && sm < 32) {
				const float lhs = BitsToF32(extRegs[sd]);
				const float rhs = BitsToF32(extRegs[sm]);
				fpscr &= ~0xF0000000u;
				if (std::isnan(lhs) || std::isnan(rhs)) {
					fpscr |= FPSCR::Zero | FPSCR::Carry | FPSCR::Overflow;
					apply_exceptions(FE_INVALID, 0);
				} else if (lhs == rhs) {
					fpscr |= FPSCR::Zero | FPSCR::Carry;
				} else if (lhs < rhs) {
					fpscr |= FPSCR::Sign;
				} else {
					fpscr |= FPSCR::Carry;
				}
			} else handled = false;
			break;
		case OpcodeID::VCVT_S32_F32:
			if (sd < 32 && sm < 32) {
				std::feclearexcept(FE_ALL_EXCEPT);
				const float in = BitsToF32(extRegs[sm]);
				const s32 out = static_cast<s32>(in);
				extRegs[sd] = static_cast<u32>(out);
				apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT), 0);
			} else handled = false;
			break;
		case OpcodeID::VCVT_U32_F32:
			if (sd < 32 && sm < 32) {
				std::feclearexcept(FE_ALL_EXCEPT);
				const float in = BitsToF32(extRegs[sm]);
				const u32 out = static_cast<u32>(in);
				extRegs[sd] = out;
				apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT), 0);
			} else handled = false;
			break;
		case OpcodeID::VCVT_F32_F64:
			if (sd < 32 && dm < 32) {
				const u64 bits = (static_cast<u64>(extRegs[GetSingleIndexFromD(dm, 1)]) << 32) |
								 extRegs[GetSingleIndexFromD(dm, 0)];
				std::feclearexcept(FE_ALL_EXCEPT);
				extRegs[sd] = F32ToBits(static_cast<float>(BitsToF64(bits)));
				apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT), 0);
			} else handled = false;
			break;
		case OpcodeID::VCVT_F64_F32:
			if (dd < 32 && sm < 32) {
				std::feclearexcept(FE_ALL_EXCEPT);
				const u64 out = F64ToBits(static_cast<double>(BitsToF32(extRegs[sm])));
				extRegs[GetSingleIndexFromD(dd, 0)] = static_cast<u32>(out);
				extRegs[GetSingleIndexFromD(dd, 1)] = static_cast<u32>(out >> 32);
				apply_exceptions(std::fetestexcept(FE_ALL_EXCEPT), 0);
			} else handled = false;
			break;
		default:
			handled = false;
			break;
	}

	if (!handled) {
		return executeCoproc(inst);
	}

	gprs[PC_INDEX] = old_pc + 4;
	return 1;
}

u32 CPU::executeNeon(u32 inst) {
	const u32 cond = inst >> 28;
	if (cond != 0xE && !conditionPassed(cond)) return 1;
	return executeCoproc(inst);
}
