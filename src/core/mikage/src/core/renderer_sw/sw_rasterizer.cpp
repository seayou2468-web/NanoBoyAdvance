#include "../../../include/renderer_sw/sw_rasterizer.hpp"

#include "../../../include/PICA/gpu.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

struct RGBA8 {
	u8 r, g, b, a;
};

struct RGBAf {
	float r, g, b, a;
};

struct ScreenVertex {
	float x;
	float y;
	float z;
	float u;
	float v;
	RGBA8 color;
};

enum class WrapMode : u32 {
	ClampToEdge = 0,
	ClampToBorder = 1,
	Repeat = 2,
	Mirror = 3,
};

enum class StencilOp : u32 {
	Keep = 0,
	Zero = 1,
	Replace = 2,
	Increment = 3,
	Decrement = 4,
	Invert = 5,
	IncrementWrap = 6,
	DecrementWrap = 7,
};

static inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
static inline u8 clampByte(float v) { return static_cast<u8>(std::clamp(v, 0.0f, 255.0f)); }

static inline RGBAf toFloat(const RGBA8& c) {
	return RGBAf{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

static inline RGBA8 toByte(const RGBAf& c) {
	return RGBA8{clampByte(c.r * 255.0f), clampByte(c.g * 255.0f), clampByte(c.b * 255.0f), clampByte(c.a * 255.0f)};
}

static inline u32 interleave3(u32 v) {
	v &= 0x7;
	v = (v ^ (v << 2)) & 0x13;
	v = (v ^ (v << 1)) & 0x15;
	return v;
}

static inline u32 getTiledOffset(u32 x, u32 y, u32 width, u32 bpp) {
	const u32 coarseX = x & ~7u;
	const u32 coarseY = y & ~7u;
	const u32 blockBase = (coarseY * width + coarseX * 8u) * bpp;
	const u32 morton = ((interleave3(y) << 1) | interleave3(x)) * bpp;
	return blockBase + morton;
}

static RGBA8 toRGBA8(const PICA::Vertex& v) {
	const float r = clamp01(v.s.colour[0].toFloat32());
	const float g = clamp01(v.s.colour[1].toFloat32());
	const float b = clamp01(v.s.colour[2].toFloat32());
	const float a = clamp01(v.s.colour[3].toFloat32());
	return RGBA8{static_cast<u8>(r * 255.0f), static_cast<u8>(g * 255.0f), static_cast<u8>(b * 255.0f), static_cast<u8>(a * 255.0f)};
}

static inline void writeColor(PICA::ColorFmt fmt, u8* dst, const RGBA8& c) {
	switch (fmt) {
		case PICA::ColorFmt::RGBA8:
			dst[0] = c.a;
			dst[1] = c.b;
			dst[2] = c.g;
			dst[3] = c.r;
			return;
		case PICA::ColorFmt::RGB8:
			dst[0] = c.b;
			dst[1] = c.g;
			dst[2] = c.r;
			return;
		case PICA::ColorFmt::RGB565: {
			const u16 r = static_cast<u16>((c.r * 31 + 127) / 255);
			const u16 g = static_cast<u16>((c.g * 63 + 127) / 255);
			const u16 b = static_cast<u16>((c.b * 31 + 127) / 255);
			const u16 raw = static_cast<u16>((r << 11) | (g << 5) | b);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		case PICA::ColorFmt::RGBA5551: {
			const u16 r = static_cast<u16>((c.r * 31 + 127) / 255);
			const u16 g = static_cast<u16>((c.g * 31 + 127) / 255);
			const u16 b = static_cast<u16>((c.b * 31 + 127) / 255);
			const u16 a = c.a >= 128 ? 1 : 0;
			const u16 raw = static_cast<u16>((r << 11) | (g << 6) | (b << 1) | a);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		case PICA::ColorFmt::RGBA4: {
			const u16 r = static_cast<u16>((c.r * 15 + 127) / 255);
			const u16 g = static_cast<u16>((c.g * 15 + 127) / 255);
			const u16 b = static_cast<u16>((c.b * 15 + 127) / 255);
			const u16 a = static_cast<u16>((c.a * 15 + 127) / 255);
			const u16 raw = static_cast<u16>((r << 12) | (g << 8) | (b << 4) | a);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		default: return;
	}
}

static inline float edge(float ax, float ay, float bx, float by, float px, float py) {
	return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static bool toScreen(const PICA::Vertex& in, u32 width, u32 height, ScreenVertex& out) {
	const float w = in.s.positions[3].toFloat32();
	if (std::abs(w) < 1e-8f) {
		return false;
	}

	const float ndcX = in.s.positions[0].toFloat32() / w;
	const float ndcY = in.s.positions[1].toFloat32() / w;
	const float ndcZ = in.s.positions[2].toFloat32() / w;

	out.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1);
	out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height - 1);
	out.z = ndcZ * 0.5f + 0.5f;
	out.u = in.s.texcoord0[0].toFloat32();
	out.v = in.s.texcoord0[1].toFloat32();
	out.color = toRGBA8(in);
	return true;
}

static inline bool compare(PICA::CompareFunction fn, u32 lhs, u32 rhs) {
	switch (fn) {
		case PICA::CompareFunction::Never: return false;
		case PICA::CompareFunction::Always: return true;
		case PICA::CompareFunction::Equal: return lhs == rhs;
		case PICA::CompareFunction::NotEqual: return lhs != rhs;
		case PICA::CompareFunction::Less: return lhs < rhs;
		case PICA::CompareFunction::LessOrEqual: return lhs <= rhs;
		case PICA::CompareFunction::Greater: return lhs > rhs;
		case PICA::CompareFunction::GreaterOrEqual: return lhs >= rhs;
		default: return true;
	}
}

static inline float readDepth(PICA::DepthFmt fmt, const u8* p) {
	switch (fmt) {
		case PICA::DepthFmt::Depth16: {
			const u16 raw = static_cast<u16>(p[0] | (u16(p[1]) << 8));
			return float(raw) / 65535.0f;
		}
		case PICA::DepthFmt::Depth24:
		case PICA::DepthFmt::Depth24Stencil8: {
			const u32 raw = u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16);
			return float(raw) / 16777215.0f;
		}
		default: return 1.0f;
	}
}

static inline void writeDepth(PICA::DepthFmt fmt, u8* p, float depth) {
	depth = std::clamp(depth, 0.0f, 1.0f);
	switch (fmt) {
		case PICA::DepthFmt::Depth16: {
			const u16 raw = static_cast<u16>(depth * 65535.0f);
			p[0] = static_cast<u8>(raw & 0xFF);
			p[1] = static_cast<u8>(raw >> 8);
			return;
		}
		case PICA::DepthFmt::Depth24:
		case PICA::DepthFmt::Depth24Stencil8: {
			const u32 raw = static_cast<u32>(depth * 16777215.0f);
			p[0] = static_cast<u8>(raw & 0xFF);
			p[1] = static_cast<u8>((raw >> 8) & 0xFF);
			p[2] = static_cast<u8>((raw >> 16) & 0xFF);
			return;
		}
		default: return;
	}
}

static inline u8 applyStencilOp(StencilOp op, u8 ref, u8 current) {
	switch (op) {
		case StencilOp::Keep: return current;
		case StencilOp::Zero: return 0;
		case StencilOp::Replace: return ref;
		case StencilOp::Increment: return current == 0xFF ? 0xFF : static_cast<u8>(current + 1);
		case StencilOp::Decrement: return current == 0 ? 0 : static_cast<u8>(current - 1);
		case StencilOp::Invert: return static_cast<u8>(~current);
		case StencilOp::IncrementWrap: return static_cast<u8>(current + 1);
		case StencilOp::DecrementWrap: return static_cast<u8>(current - 1);
		default: return current;
	}
}

static inline RGBA8 readTexel(PICA::TextureFmt fmt, const u8* p) {
	switch (fmt) {
		case PICA::TextureFmt::RGBA8: return RGBA8{p[3], p[2], p[1], p[0]};
		case PICA::TextureFmt::RGB8: return RGBA8{p[2], p[1], p[0], 0xFF};
		case PICA::TextureFmt::RGBA5551: {
			const u16 raw = u16(p[0]) | (u16(p[1]) << 8);
			return RGBA8{u8(((raw >> 11) & 0x1F) * 255 / 31), u8(((raw >> 6) & 0x1F) * 255 / 31),
			             u8(((raw >> 1) & 0x1F) * 255 / 31), u8((raw & 1) ? 0xFF : 0x00)};
		}
		case PICA::TextureFmt::RGB565: {
			const u16 raw = u16(p[0]) | (u16(p[1]) << 8);
			return RGBA8{u8(((raw >> 11) & 0x1F) * 255 / 31), u8(((raw >> 5) & 0x3F) * 255 / 63), u8((raw & 0x1F) * 255 / 31), 0xFF};
		}
		case PICA::TextureFmt::RGBA4: {
			const u16 raw = u16(p[0]) | (u16(p[1]) << 8);
			return RGBA8{u8(((raw >> 12) & 0xF) * 17), u8(((raw >> 8) & 0xF) * 17), u8(((raw >> 4) & 0xF) * 17), u8((raw & 0xF) * 17)};
		}
		case PICA::TextureFmt::IA8: return RGBA8{p[1], p[1], p[1], p[0]};
		case PICA::TextureFmt::RG8: return RGBA8{p[1], p[0], 0x00, 0xFF};
		case PICA::TextureFmt::I8: return RGBA8{p[0], p[0], p[0], 0xFF};
		case PICA::TextureFmt::A8: return RGBA8{0xFF, 0xFF, 0xFF, p[0]};
		case PICA::TextureFmt::IA4: {
			const u8 i = (p[0] >> 4) * 17;
			const u8 a = (p[0] & 0xF) * 17;
			return RGBA8{i, i, i, a};
		}
		case PICA::TextureFmt::I4: {
			const u8 i = (p[0] & 0xF) * 17;
			return RGBA8{i, i, i, 0xFF};
		}
		case PICA::TextureFmt::A4: {
			const u8 a = (p[0] & 0xF) * 17;
			return RGBA8{0xFF, 0xFF, 0xFF, a};
		}
		// ETC formats are TODO: return debug magenta to reveal unsupported path.
		case PICA::TextureFmt::ETC1:
		case PICA::TextureFmt::ETC1A4:
		default: return RGBA8{255, 0, 255, 255};
	}
}

static inline float wrapCoord(float t, WrapMode mode) {
	switch (mode) {
		case WrapMode::ClampToEdge:
		case WrapMode::ClampToBorder: return std::clamp(t, 0.0f, 1.0f);
		case WrapMode::Repeat: {
			float f = t - std::floor(t);
			return f < 0.0f ? f + 1.0f : f;
		}
		case WrapMode::Mirror: {
			float f = std::fmod(std::abs(t), 2.0f);
			if (f > 1.0f) f = 2.0f - f;
			return f;
		}
		default: return t;
	}
}

static inline RGBAf applyColorOperand(PICA::TexEnvConfig::ColorOperand op, const RGBAf& s) {
	switch (op) {
		case PICA::TexEnvConfig::ColorOperand::SourceColor: return RGBAf{s.r, s.g, s.b, s.a};
		case PICA::TexEnvConfig::ColorOperand::OneMinusSourceColor: return RGBAf{1.0f - s.r, 1.0f - s.g, 1.0f - s.b, s.a};
		case PICA::TexEnvConfig::ColorOperand::SourceAlpha: return RGBAf{s.a, s.a, s.a, s.a};
		case PICA::TexEnvConfig::ColorOperand::OneMinusSourceAlpha: return RGBAf{1.0f - s.a, 1.0f - s.a, 1.0f - s.a, s.a};
		case PICA::TexEnvConfig::ColorOperand::SourceRed: return RGBAf{s.r, s.r, s.r, s.a};
		case PICA::TexEnvConfig::ColorOperand::OneMinusSourceRed: return RGBAf{1.0f - s.r, 1.0f - s.r, 1.0f - s.r, s.a};
		case PICA::TexEnvConfig::ColorOperand::SourceGreen: return RGBAf{s.g, s.g, s.g, s.a};
		case PICA::TexEnvConfig::ColorOperand::OneMinusSourceGreen: return RGBAf{1.0f - s.g, 1.0f - s.g, 1.0f - s.g, s.a};
		case PICA::TexEnvConfig::ColorOperand::SourceBlue: return RGBAf{s.b, s.b, s.b, s.a};
		case PICA::TexEnvConfig::ColorOperand::OneMinusSourceBlue: return RGBAf{1.0f - s.b, 1.0f - s.b, 1.0f - s.b, s.a};
		default: return s;
	}
}

static inline float applyAlphaOperand(PICA::TexEnvConfig::AlphaOperand op, const RGBAf& s) {
	switch (op) {
		case PICA::TexEnvConfig::AlphaOperand::SourceAlpha: return s.a;
		case PICA::TexEnvConfig::AlphaOperand::OneMinusSourceAlpha: return 1.0f - s.a;
		case PICA::TexEnvConfig::AlphaOperand::SourceRed: return s.r;
		case PICA::TexEnvConfig::AlphaOperand::OneMinusSourceRed: return 1.0f - s.r;
		case PICA::TexEnvConfig::AlphaOperand::SourceGreen: return s.g;
		case PICA::TexEnvConfig::AlphaOperand::OneMinusSourceGreen: return 1.0f - s.g;
		case PICA::TexEnvConfig::AlphaOperand::SourceBlue: return s.b;
		case PICA::TexEnvConfig::AlphaOperand::OneMinusSourceBlue: return 1.0f - s.b;
		default: return s.a;
	}
}

static RGBAf combineColor(PICA::TexEnvConfig::Operation op, const RGBAf& a, const RGBAf& b, const RGBAf& c) {
	RGBAf out{};
	switch (op) {
		case PICA::TexEnvConfig::Operation::Replace: out = a; break;
		case PICA::TexEnvConfig::Operation::Modulate: out = RGBAf{a.r * b.r, a.g * b.g, a.b * b.b, a.a}; break;
		case PICA::TexEnvConfig::Operation::Add: out = RGBAf{a.r + b.r, a.g + b.g, a.b + b.b, a.a}; break;
		case PICA::TexEnvConfig::Operation::AddSigned: out = RGBAf{a.r + b.r - 0.5f, a.g + b.g - 0.5f, a.b + b.b - 0.5f, a.a}; break;
		case PICA::TexEnvConfig::Operation::Lerp: out = RGBAf{a.r * c.r + b.r * (1.0f - c.r), a.g * c.g + b.g * (1.0f - c.g),
		                                                    a.b * c.b + b.b * (1.0f - c.b), a.a};
			break;
		case PICA::TexEnvConfig::Operation::Subtract: out = RGBAf{a.r - b.r, a.g - b.g, a.b - b.b, a.a}; break;
		case PICA::TexEnvConfig::Operation::Dot3RGB:
		case PICA::TexEnvConfig::Operation::Dot3RGBA: {
			const float d = std::clamp((a.r - 0.5f) * (b.r - 0.5f) + (a.g - 0.5f) * (b.g - 0.5f) + (a.b - 0.5f) * (b.b - 0.5f), 0.0f, 1.0f);
			out = RGBAf{d, d, d, (op == PICA::TexEnvConfig::Operation::Dot3RGBA) ? d : a.a};
			break;
		}
		case PICA::TexEnvConfig::Operation::MultiplyAdd: out = RGBAf{a.r * b.r + c.r, a.g * b.g + c.g, a.b * b.b + c.b, a.a}; break;
		case PICA::TexEnvConfig::Operation::AddMultiply: out = RGBAf{(a.r + b.r) * c.r, (a.g + b.g) * c.g, (a.b + b.b) * c.b, a.a}; break;
		default: out = a; break;
	}
	out.r = clamp01(out.r);
	out.g = clamp01(out.g);
	out.b = clamp01(out.b);
	out.a = clamp01(out.a);
	return out;
}

static float combineAlpha(PICA::TexEnvConfig::Operation op, float a, float b, float c) {
	float out = a;
	switch (op) {
		case PICA::TexEnvConfig::Operation::Replace: out = a; break;
		case PICA::TexEnvConfig::Operation::Modulate: out = a * b; break;
		case PICA::TexEnvConfig::Operation::Add: out = a + b; break;
		case PICA::TexEnvConfig::Operation::AddSigned: out = a + b - 0.5f; break;
		case PICA::TexEnvConfig::Operation::Lerp: out = a * c + b * (1.0f - c); break;
		case PICA::TexEnvConfig::Operation::Subtract: out = a - b; break;
		case PICA::TexEnvConfig::Operation::MultiplyAdd: out = a * b + c; break;
		case PICA::TexEnvConfig::Operation::AddMultiply: out = (a + b) * c; break;
		default: out = a; break;
	}
	return clamp01(out);
}

}  // namespace

void SwRasterizer::drawTriangles(
	u32 colorBufferLoc, PICA::ColorFmt colorFormat, u32 depthBufferLoc, PICA::DepthFmt depthFormat, std::array<u32, 2> fbSize,
	std::span<const PICA::Vertex> vertices
) {
	const u32 width = fbSize[0];
	const u32 height = fbSize[1];
	if (width == 0 || height == 0 || vertices.size() < 3) {
		return;
	}

	auto& regs = gpu.getRegisters();

	const bool depthTestEnable = (regs[PICA::InternalRegs::DepthAndColorMask] & 1u) != 0;
	const bool depthWriteEnable = (regs[PICA::InternalRegs::DepthBufferWrite] & 1u) != 0;
	const auto depthFn = static_cast<PICA::CompareFunction>((regs[PICA::InternalRegs::DepthAndColorMask] >> 4) & 0x7);

	const u32 stencilTest = regs[PICA::InternalRegs::StencilTest];
	const u32 stencilOp = regs[PICA::InternalRegs::StencilOp];
	const bool stencilEnable = (stencilTest & 1u) != 0 && depthFormat == PICA::DepthFmt::Depth24Stencil8;
	const auto stencilFn = static_cast<PICA::CompareFunction>((stencilTest >> 4) & 0x7);
	const u8 stencilRef = static_cast<u8>((stencilTest >> 16) & 0xFF);
	const u8 stencilMask = static_cast<u8>((stencilTest >> 24) & 0xFF);
	const u8 stencilWriteMask = static_cast<u8>((stencilTest >> 8) & 0xFF);
	const auto opFail = static_cast<StencilOp>((stencilOp >> 0) & 0x7);
	const auto opZFail = static_cast<StencilOp>((stencilOp >> 4) & 0x7);
	const auto opZPass = static_cast<StencilOp>((stencilOp >> 8) & 0x7);

	const bool textureEnable = (regs[PICA::InternalRegs::TexUnitCfg] & 1u) != 0;
	const u32 texAddrReg = regs[0x85];
	const u32 texSizeReg = regs[0x82];
	const u32 texParamReg = regs[0x83];
	const u32 texAddr = (texAddrReg & 0x0FFFFFFF) << 3;
	const u32 texWidth = (texSizeReg & 0x7FF) + 1;
	const u32 texHeight = ((texSizeReg >> 16) & 0x7FF) + 1;
	const auto texFormat = static_cast<PICA::TextureFmt>(texParamReg & 0xF);
	const u32 texBpp = static_cast<u32>(PICA::sizePerPixel(texFormat));
	const WrapMode wrapS = static_cast<WrapMode>((texParamReg >> 8) & 0x3);
	const WrapMode wrapT = static_cast<WrapMode>((texParamReg >> 12) & 0x3);
	u8* tex = (textureEnable && texAddr != 0 && texWidth > 0 && texHeight > 0 && texBpp > 0) ? gpu.getPointerPhys<u8>(texAddr) : nullptr;

	std::array<PICA::TexEnvConfig, 6> tev = {
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv0Source], regs[PICA::InternalRegs::TexEnv0Operand], regs[PICA::InternalRegs::TexEnv0Combiner], regs[PICA::InternalRegs::TexEnv0Color], regs[PICA::InternalRegs::TexEnv0Scale]),
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv1Source], regs[PICA::InternalRegs::TexEnv1Operand], regs[PICA::InternalRegs::TexEnv1Combiner], regs[PICA::InternalRegs::TexEnv1Color], regs[PICA::InternalRegs::TexEnv1Scale]),
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv2Source], regs[PICA::InternalRegs::TexEnv2Operand], regs[PICA::InternalRegs::TexEnv2Combiner], regs[PICA::InternalRegs::TexEnv2Color], regs[PICA::InternalRegs::TexEnv2Scale]),
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv3Source], regs[PICA::InternalRegs::TexEnv3Operand], regs[PICA::InternalRegs::TexEnv3Combiner], regs[PICA::InternalRegs::TexEnv3Color], regs[PICA::InternalRegs::TexEnv3Scale]),
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv4Source], regs[PICA::InternalRegs::TexEnv4Operand], regs[PICA::InternalRegs::TexEnv4Combiner], regs[PICA::InternalRegs::TexEnv4Color], regs[PICA::InternalRegs::TexEnv4Scale]),
		PICA::TexEnvConfig(regs[PICA::InternalRegs::TexEnv5Source], regs[PICA::InternalRegs::TexEnv5Operand], regs[PICA::InternalRegs::TexEnv5Combiner], regs[PICA::InternalRegs::TexEnv5Color], regs[PICA::InternalRegs::TexEnv5Scale]),
	};

	const u32 colorBpp = static_cast<u32>(PICA::sizePerPixel(colorFormat));
	if (colorBpp == 0) {
		return;
	}

	u8* colorBuffer = gpu.getPointerPhys<u8>(PhysicalAddrs::VRAM + colorBufferLoc);
	if (colorBuffer == nullptr) {
		return;
	}

	const u32 depthBpp = static_cast<u32>(PICA::sizePerPixel(depthFormat));
	u8* depthBuffer = gpu.getPointerPhys<u8>(PhysicalAddrs::VRAM + depthBufferLoc);

	auto sourceColor = [&](PICA::TexEnvConfig::Source src, const RGBAf& primary, const RGBAf& texture0, const RGBAf& constant, const RGBAf& previous) {
		switch (src) {
			case PICA::TexEnvConfig::Source::PrimaryColor:
			case PICA::TexEnvConfig::Source::PrimaryFragmentColor: return primary;
			case PICA::TexEnvConfig::Source::Texture0: return texture0;
			case PICA::TexEnvConfig::Source::Constant: return constant;
			case PICA::TexEnvConfig::Source::Previous:
			case PICA::TexEnvConfig::Source::PreviousBuffer:
			default: return previous;
		}
	};

	auto emitTriangle = [&](const PICA::Vertex& a, const PICA::Vertex& b, const PICA::Vertex& c) {
		ScreenVertex v0{}, v1{}, v2{};
		if (!toScreen(a, width, height, v0) || !toScreen(b, width, height, v1) || !toScreen(c, width, height, v2)) {
			return;
		}

		const float area = edge(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
		if (std::abs(area) < 1e-8f) {
			return;
		}

		const float minXf = std::floor(std::min({v0.x, v1.x, v2.x}));
		const float maxXf = std::ceil(std::max({v0.x, v1.x, v2.x}));
		const float minYf = std::floor(std::min({v0.y, v1.y, v2.y}));
		const float maxYf = std::ceil(std::max({v0.y, v1.y, v2.y}));

		const u32 minX = static_cast<u32>(std::max(0.0f, minXf));
		const u32 maxX = static_cast<u32>(std::min(maxXf, static_cast<float>(width - 1)));
		const u32 minY = static_cast<u32>(std::max(0.0f, minYf));
		const u32 maxY = static_cast<u32>(std::min(maxYf, static_cast<float>(height - 1)));

		for (u32 y = minY; y <= maxY; ++y) {
			for (u32 x = minX; x <= maxX; ++x) {
				const float px = static_cast<float>(x) + 0.5f;
				const float py = static_cast<float>(y) + 0.5f;

				const float w0 = edge(v1.x, v1.y, v2.x, v2.y, px, py) / area;
				const float w1 = edge(v2.x, v2.y, v0.x, v0.y, px, py) / area;
				const float w2 = edge(v0.x, v0.y, v1.x, v1.y, px, py) / area;
				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
					continue;
				}

				const u32 zOff = getTiledOffset(x, y, width, depthBpp);
				u8* zPtr = (depthBuffer && depthBpp > 0) ? (depthBuffer + zOff) : nullptr;
				u8 stencilCur = (zPtr && depthFormat == PICA::DepthFmt::Depth24Stencil8) ? zPtr[3] : 0;

				bool stencilPassed = true;
				if (stencilEnable) {
					const u32 refMasked = stencilRef & stencilMask;
					const u32 curMasked = stencilCur & stencilMask;
					stencilPassed = compare(stencilFn, refMasked, curMasked);
					if (!stencilPassed) {
						const u8 next = applyStencilOp(opFail, stencilRef, stencilCur);
						zPtr[3] = (stencilCur & ~stencilWriteMask) | (next & stencilWriteMask);
						continue;
					}
				}

				const float z = std::clamp(w0 * v0.z + w1 * v1.z + w2 * v2.z, 0.0f, 1.0f);
				if (depthTestEnable && zPtr != nullptr) {
					const float zCur = readDepth(depthFormat, zPtr);
					const u32 zInt = static_cast<u32>(z * 16777215.0f);
					const u32 zCurInt = static_cast<u32>(zCur * 16777215.0f);
					const bool depthPass = compare(depthFn, zInt, zCurInt);
					if (!depthPass) {
						if (stencilEnable) {
							const u8 next = applyStencilOp(opZFail, stencilRef, stencilCur);
							zPtr[3] = (stencilCur & ~stencilWriteMask) | (next & stencilWriteMask);
						}
						continue;
					}
					if (depthWriteEnable) {
						writeDepth(depthFormat, zPtr, z);
					}
				}

				if (stencilEnable) {
					const u8 cur2 = zPtr[3];
					const u8 next = applyStencilOp(opZPass, stencilRef, cur2);
					zPtr[3] = (cur2 & ~stencilWriteMask) | (next & stencilWriteMask);
				}

				RGBA8 primary;
				primary.r = static_cast<u8>(std::clamp(w0 * v0.color.r + w1 * v1.color.r + w2 * v2.color.r, 0.0f, 255.0f));
				primary.g = static_cast<u8>(std::clamp(w0 * v0.color.g + w1 * v1.color.g + w2 * v2.color.g, 0.0f, 255.0f));
				primary.b = static_cast<u8>(std::clamp(w0 * v0.color.b + w1 * v1.color.b + w2 * v2.color.b, 0.0f, 255.0f));
				primary.a = static_cast<u8>(std::clamp(w0 * v0.color.a + w1 * v1.color.a + w2 * v2.color.a, 0.0f, 255.0f));

				RGBA8 texel{255, 255, 255, 255};
				if (tex != nullptr) {
					float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
					float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;
					u = wrapCoord(u, wrapS);
					v = wrapCoord(v, wrapT);
					const u32 tx = std::clamp(static_cast<int>(u * float(texWidth)), 0, int(texWidth - 1));
					const u32 ty = std::clamp(static_cast<int>(v * float(texHeight)), 0, int(texHeight - 1));
					const u32 toff = (ty * texWidth + tx) * texBpp;
					texel = readTexel(texFormat, tex + toff);
				}

				RGBAf previous = toFloat(primary);
				const RGBAf texture0 = toFloat(texel);
				const RGBAf const0 = RGBAf{((regs[PICA::InternalRegs::TexEnv0Color] >> 24) & 0xFF) / 255.0f,
				                         ((regs[PICA::InternalRegs::TexEnv0Color] >> 16) & 0xFF) / 255.0f,
				                         ((regs[PICA::InternalRegs::TexEnv0Color] >> 8) & 0xFF) / 255.0f,
				                         (regs[PICA::InternalRegs::TexEnv0Color] & 0xFF) / 255.0f};
				for (int i = 0; i < 6; ++i) {
					auto& s = tev[i];
					if (s.isPassthroughStage()) {
						continue;
					}

					const RGBAf ca = applyColorOperand(s.colorOperand1, sourceColor(s.colorSource1, toFloat(primary), texture0, const0, previous));
					const RGBAf cb = applyColorOperand(s.colorOperand2, sourceColor(s.colorSource2, toFloat(primary), texture0, const0, previous));
					const RGBAf cc = applyColorOperand(s.colorOperand3, sourceColor(s.colorSource3, toFloat(primary), texture0, const0, previous));

					const float aa = applyAlphaOperand(s.alphaOperand1, sourceColor(s.alphaSource1, toFloat(primary), texture0, const0, previous));
					const float ab = applyAlphaOperand(s.alphaOperand2, sourceColor(s.alphaSource2, toFloat(primary), texture0, const0, previous));
					const float ac = applyAlphaOperand(s.alphaOperand3, sourceColor(s.alphaSource3, toFloat(primary), texture0, const0, previous));

					RGBAf out = combineColor(s.colorOp, ca, cb, cc);
					out.a = combineAlpha(s.alphaOp, aa, ab, ac);
					const float cscale = float(s.getColorScale());
					const float ascale = float(s.getAlphaScale());
					out.r = clamp01(out.r * cscale);
					out.g = clamp01(out.g * cscale);
					out.b = clamp01(out.b * cscale);
					out.a = clamp01(out.a * ascale);
					previous = out;
				}

				const RGBA8 out = toByte(previous);
				const u32 cOff = getTiledOffset(x, y, width, colorBpp);
				writeColor(colorFormat, colorBuffer + cOff, out);
			}
		}
	};

	for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
		emitTriangle(vertices[i], vertices[i + 1], vertices[i + 2]);
	}
}
