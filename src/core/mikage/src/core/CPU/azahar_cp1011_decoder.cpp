#include "./azahar_cp1011_decoder.hpp"

#include <array>

namespace {
struct Rule {
	u8 lo;
	u8 hi;
	u32 value;
};

struct Pattern {
	AzaharCp1011Decoder::Op op;
	std::array<Rule, 5> rules;
	u8 count;
};

constexpr bool MatchRule(u32 opcode, const Rule& rule) {
	const u32 width = static_cast<u32>(rule.hi - rule.lo + 1);
	const u32 mask = width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1u);
	return ((opcode >> rule.lo) & mask) == rule.value;
}

constexpr std::array<Pattern, 32> kPatterns = {{
	{AzaharCp1011Decoder::Op::Vmla, {{{23, 27, 0x1C}, {20, 21, 0x0}, {9, 11, 0x5}, {6, 6, 0}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vmls, {{{23, 27, 0x1C}, {20, 21, 0x0}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vnmla, {{{23, 27, 0x1C}, {20, 21, 0x1}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vnmls, {{{23, 27, 0x1C}, {20, 21, 0x1}, {9, 11, 0x5}, {6, 6, 0}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vnmul, {{{23, 27, 0x1C}, {20, 21, 0x2}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vmul, {{{23, 27, 0x1C}, {20, 21, 0x2}, {9, 11, 0x5}, {6, 6, 0}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vadd, {{{23, 27, 0x1C}, {20, 21, 0x3}, {9, 11, 0x5}, {6, 6, 0}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vsub, {{{23, 27, 0x1C}, {20, 21, 0x3}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vdiv, {{{23, 27, 0x1D}, {20, 21, 0x0}, {9, 11, 0x5}, {6, 6, 0}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::VmovImm, {{{23, 27, 0x1D}, {20, 21, 0x3}, {9, 11, 0x5}, {4, 7, 0x0}, {0, 0, 0}}}, 4},
	{AzaharCp1011Decoder::Op::VmovReg, {{{23, 27, 0x1D}, {16, 21, 0x30}, {9, 11, 0x5}, {6, 7, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vabs, {{{23, 27, 0x1D}, {16, 21, 0x30}, {9, 11, 0x5}, {6, 7, 3}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vneg, {{{23, 27, 0x1D}, {17, 21, 0x18}, {9, 11, 0x5}, {6, 7, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vsqrt, {{{23, 27, 0x1D}, {16, 21, 0x31}, {9, 11, 0x5}, {6, 7, 3}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::Vcmp, {{{23, 27, 0x1D}, {16, 21, 0x34}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::VcmpZero, {{{23, 27, 0x1D}, {16, 21, 0x35}, {9, 11, 0x5}, {0, 6, 0x40}, {0, 0, 0}}}, 4},
	{AzaharCp1011Decoder::Op::VcvtBds, {{{23, 27, 0x1D}, {16, 21, 0x37}, {9, 11, 0x5}, {6, 7, 3}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::VcvtBff, {{{23, 27, 0x1D}, {19, 21, 0x7}, {17, 17, 0x1}, {9, 11, 5}, {6, 6, 1}}}, 5},
	{AzaharCp1011Decoder::Op::VcvtBfi, {{{23, 27, 0x1D}, {19, 21, 0x7}, {9, 11, 0x5}, {6, 6, 1}, {4, 4, 0}}}, 5},
	{AzaharCp1011Decoder::Op::VmovBrs, {{{21, 27, 0x70}, {8, 11, 0xA}, {0, 6, 0x10}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vmsr, {{{20, 27, 0xEE}, {0, 11, 0xA10}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
	{AzaharCp1011Decoder::Op::VmovBrc, {{{23, 27, 0x1C}, {20, 20, 0x0}, {8, 11, 0xB}, {0, 4, 0x10}, {0, 0, 0}}}, 4},
	{AzaharCp1011Decoder::Op::Vmrs, {{{20, 27, 0xEF}, {0, 11, 0xA10}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}, 2},
	{AzaharCp1011Decoder::Op::VmovBcr, {{{24, 27, 0xE}, {20, 20, 1}, {8, 11, 0xB}, {0, 4, 0x10}, {0, 0, 0}}}, 4},
	{AzaharCp1011Decoder::Op::VmovBrrss, {{{21, 27, 0x62}, {8, 11, 0xA}, {4, 4, 1}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::VmovBrrd, {{{21, 27, 0x62}, {6, 11, 0x2C}, {4, 4, 1}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vstr, {{{24, 27, 0xD}, {20, 21, 0}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vpush, {{{23, 27, 0x1A}, {16, 21, 0x2D}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vstm, {{{25, 27, 0x6}, {20, 20, 0}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vpop, {{{23, 27, 0x19}, {16, 21, 0x3D}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vldr, {{{24, 27, 0xD}, {20, 21, 1}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
	{AzaharCp1011Decoder::Op::Vldm, {{{25, 27, 0x6}, {20, 20, 1}, {9, 11, 5}, {0, 0, 0}, {0, 0, 0}}}, 3},
}};
}  // namespace

AzaharCp1011Decoder::Op AzaharCp1011Decoder::Decode(u32 opcode) {
	for (const auto& pattern : kPatterns) {
		bool ok = true;
		for (u8 i = 0; i < pattern.count; i++) {
			if (!MatchRule(opcode, pattern.rules[i])) {
				ok = false;
				break;
			}
		}
		if (ok) {
			return pattern.op;
		}
	}
	return Op::Unknown;
}
