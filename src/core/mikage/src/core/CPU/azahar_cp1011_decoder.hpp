#pragma once

#include "../../../include/arm_defs.hpp"

class AzaharCp1011Decoder {
  public:
	enum class Op : u8 {
		Unknown,
		Vmla,
		Vmls,
		Vnmla,
		Vnmls,
		Vnmul,
		Vmul,
		Vadd,
		Vsub,
		Vdiv,
		VmovImm,
		VmovReg,
		Vabs,
		Vneg,
		Vsqrt,
		Vcmp,
		VcmpZero,
		VcvtBds,
		VcvtBff,
		VcvtBfi,
		VmovBrs,
		Vmsr,
		VmovBrc,
		Vmrs,
		VmovBcr,
		VmovBrrss,
		VmovBrrd,
		Vstr,
		Vpush,
		Vstm,
		Vpop,
		Vldr,
		Vldm,
	};

	static Op Decode(u32 opcode);
};
