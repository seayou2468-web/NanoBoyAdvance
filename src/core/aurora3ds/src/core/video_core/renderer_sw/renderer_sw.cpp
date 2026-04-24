#include "../../../../include/renderer_sw/renderer_sw.hpp"
#include "../../../../include/PICA/gpu.hpp"
#include "../../../../include/io_file.hpp"
#include "../../../../include/renderer_sw/sw_rasterizer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

namespace {

struct RGBA {
	u8 r = 0;
	u8 g = 0;
	u8 b = 0;
	u8 a = 0xFF;
};

static inline u32 expand5To8(u32 v) { return (v << 3) | (v >> 2); }
static inline u32 expand6To8(u32 v) { return (v << 2) | (v >> 4); }
static inline u32 expand4To8(u32 v) { return (v << 4) | v; }

static inline u16 pack565(const RGBA& c) {
	const u16 r = static_cast<u16>((c.r * 31 + 127) / 255);
	const u16 g = static_cast<u16>((c.g * 63 + 127) / 255);
	const u16 b = static_cast<u16>((c.b * 31 + 127) / 255);
	return static_cast<u16>((r << 11) | (g << 5) | b);
}

static inline u16 pack5551(const RGBA& c) {
	const u16 r = static_cast<u16>((c.r * 31 + 127) / 255);
	const u16 g = static_cast<u16>((c.g * 31 + 127) / 255);
	const u16 b = static_cast<u16>((c.b * 31 + 127) / 255);
	const u16 a = c.a >= 128 ? 1 : 0;
	return static_cast<u16>((r << 11) | (g << 6) | (b << 1) | a);
}

static inline u16 packRGBA4(const RGBA& c) {
	const u16 r = static_cast<u16>((c.r * 15 + 127) / 255);
	const u16 g = static_cast<u16>((c.g * 15 + 127) / 255);
	const u16 b = static_cast<u16>((c.b * 15 + 127) / 255);
	const u16 a = static_cast<u16>((c.a * 15 + 127) / 255);
	return static_cast<u16>((r << 12) | (g << 8) | (b << 4) | a);
}

static RGBA decodePixel(PICA::ColorFmt fmt, const u8* src) {
	switch (fmt) {
		case PICA::ColorFmt::RGBA8: return RGBA{src[3], src[2], src[1], src[0]};
		case PICA::ColorFmt::RGB8: return RGBA{src[2], src[1], src[0], 0xFF};
		case PICA::ColorFmt::RGB565: {
			const u16 raw = static_cast<u16>(src[0] | (u16(src[1]) << 8));
			return RGBA{(u8)expand5To8((raw >> 11) & 0x1F), (u8)expand6To8((raw >> 5) & 0x3F), (u8)expand5To8(raw & 0x1F), 0xFF};
		}
		case PICA::ColorFmt::RGBA5551: {
			const u16 raw = static_cast<u16>(src[0] | (u16(src[1]) << 8));
			return RGBA{(u8)expand5To8((raw >> 11) & 0x1F), (u8)expand5To8((raw >> 6) & 0x1F), (u8)expand5To8((raw >> 1) & 0x1F),
			            (u8)((raw & 1) ? 0xFF : 0)};
		}
		case PICA::ColorFmt::RGBA4: {
			const u16 raw = static_cast<u16>(src[0] | (u16(src[1]) << 8));
			return RGBA{(u8)expand4To8((raw >> 12) & 0xF), (u8)expand4To8((raw >> 8) & 0xF), (u8)expand4To8((raw >> 4) & 0xF),
			            (u8)expand4To8(raw & 0xF)};
		}
		default: return RGBA{};
	}
}

static void encodePixel(PICA::ColorFmt fmt, const RGBA& c, u8* dst) {
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
			const u16 raw = pack565(c);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		case PICA::ColorFmt::RGBA5551: {
			const u16 raw = pack5551(c);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		case PICA::ColorFmt::RGBA4: {
			const u16 raw = packRGBA4(c);
			dst[0] = static_cast<u8>(raw & 0xFF);
			dst[1] = static_cast<u8>(raw >> 8);
			return;
		}
		default: return;
	}
}

static RGBA avg2(const RGBA& a, const RGBA& b) {
	return RGBA{(u8)((u16(a.r) + u16(b.r)) / 2), (u8)((u16(a.g) + u16(b.g)) / 2), (u8)((u16(a.b) + u16(b.b)) / 2),
	            (u8)((u16(a.a) + u16(b.a)) / 2)};
}

static RGBA avg4(const RGBA& a, const RGBA& b, const RGBA& c, const RGBA& d) {
	return RGBA{(u8)((u32(a.r) + b.r + c.r + d.r) / 4), (u8)((u32(a.g) + b.g + c.g + d.g) / 4),
	            (u8)((u32(a.b) + b.b + c.b + d.b) / 4), (u8)((u32(a.a) + b.a + c.a + d.a) / 4)};
}

static inline u32 interleave3(u32 v) {
	v &= 0x7;
	v = (v ^ (v << 2)) & 0x13;
	v = (v ^ (v << 1)) & 0x15;
	return v;
}

static inline u32 getMortonOffset8x8(u32 x, u32 y, u32 bpp) {
	return (interleave3(y) << 1 | interleave3(x)) * bpp;
}

}  // namespace

RendererSw::RendererSw(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs), rasterizer(std::make_unique<SwRasterizer>(gpu)) {}
RendererSw::~RendererSw() {}

void RendererSw::reset() {}
void RendererSw::display() {}

void RendererSw::initGraphicsContext(void* context) {
	(void)context;
	// Software renderer does not need an external graphics context.
}

void RendererSw::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	if (endAddress <= startAddress) {
		return;
	}

	u8* start = gpu.getPointerPhys<u8>(startAddress);
	u8* end = gpu.getPointerPhys<u8>(endAddress - 1);
	if (start == nullptr || end == nullptr) {
		return;
	}

	u8* ptr = start;
	const u32 size = endAddress - startAddress;
	const u32 fillWidth = (control >> 8) & 0x3;  // 0=16bit,1/3=24bit,2=32bit (3dbrew)

	if (fillWidth == 2) {
		for (u32 i = 0; i + 3 < size; i += 4) {
			std::memcpy(ptr + i, &value, sizeof(u32));
		}
	} else if (fillWidth == 1 || fillWidth == 3) {
		const u8 r = static_cast<u8>(value & 0xFF);
		const u8 g = static_cast<u8>((value >> 8) & 0xFF);
		const u8 b = static_cast<u8>((value >> 16) & 0xFF);
		for (u32 i = 0; i + 2 < size; i += 3) {
			ptr[i + 0] = r;
			ptr[i + 1] = g;
			ptr[i + 2] = b;
		}
	} else {
		const u16 value16 = static_cast<u16>(value & 0xFFFF);
		for (u32 i = 0; i + 1 < size; i += 2) {
			std::memcpy(ptr + i, &value16, sizeof(u16));
		}
	}
}

void RendererSw::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	// This follows Azahar/Citra software blitter behavior for transfer-engine operations.
	const u32 inputWidth = inputSize & 0xFFFF;
	const u32 inputHeight = inputSize >> 16;
	const u32 outputWidthRaw = outputSize & 0xFFFF;
	const u32 outputHeightRaw = outputSize >> 16;

	if (inputWidth == 0 || inputHeight == 0 || outputWidthRaw == 0 || outputHeightRaw == 0) {
		return;
	}

	const bool flipVertically = (flags & (1u << 0)) != 0;
	const bool inputLinear = (flags & (1u << 1)) != 0;
	const bool cropInputLines = (flags & (1u << 2)) != 0;
	const bool isTextureCopy = (flags & (1u << 3)) != 0;
	const bool dontSwizzle = (flags & (1u << 5)) != 0;
	const bool block32 = (flags & (1u << 16)) != 0;

	// Azahar/Citra exposes this flag in display-transfer config. In this codebase
	// texture copy is delivered via a dedicated command path, so treat this as diagnostic.
	if (isTextureCopy) {
		Helpers::warn("RendererSw::displayTransfer: texture-copy flag set on display transfer path (flags=%08X)\n", flags);
	}
	if (block32) {
		Helpers::warn("RendererSw::displayTransfer: 32x32 block swizzle mode is not implemented in SW path (flags=%08X)\n", flags);
		return;
	}

	const u32 scalingMode = (flags >> 24) & 0x3;  // 0:none,1:2x1,2:2x2
	if (scalingMode > 2) {
		return;
	}
	if (inputLinear && scalingMode != 0) {
		Helpers::warn("RendererSw::displayTransfer: scaling with linear input is unsupported in SW path (flags=%08X)\n", flags);
		return;
	}

	const auto inputFmt = static_cast<PICA::ColorFmt>((flags >> 8) & 0x7);
	const auto outputFmt = static_cast<PICA::ColorFmt>((flags >> 12) & 0x7);
	const u32 srcBpp = static_cast<u32>(PICA::sizePerPixel(inputFmt));
	const u32 dstBpp = static_cast<u32>(PICA::sizePerPixel(outputFmt));
	if (srcBpp == 0 || dstBpp == 0) {
		return;
	}

	u32 adjustedOutputAddr = outputAddr;
	// Azahar/Citra compatibility quirk:
	// flip_vertically + crop_input_lines skews destination addressing on hardware.
	if (flipVertically && cropInputLines && inputWidth > outputWidthRaw && outputHeightRaw > 0) {
		adjustedOutputAddr += (inputWidth - outputWidthRaw) * (outputHeightRaw - 1) * dstBpp;
	}

	u8* src = gpu.getPointerPhys<u8>(inputAddr);
	u8* dst = gpu.getPointerPhys<u8>(adjustedOutputAddr);
	if (src == nullptr || dst == nullptr) {
		return;
	}

	const u32 horizontalScale = scalingMode != 0 ? 1 : 0;
	const u32 verticalScale = scalingMode == 2 ? 1 : 0;
	const u32 outputWidth = outputWidthRaw >> horizontalScale;
	const u32 outputHeight = outputHeightRaw >> verticalScale;
	if (outputWidth == 0 || outputHeight == 0) {
		return;
	}

	// Match Azahar/Citra sw blitter transfer semantics:
	// - input linear + dont_swizzle=0 => output tiled
	// - input linear + dont_swizzle=1 => output linear
	// - input tiled  + dont_swizzle=0 => output linear
	// - input tiled  + dont_swizzle=1 => output tiled
	const bool outputTiled = inputLinear ? !dontSwizzle : dontSwizzle;
	const bool sourceTiled = !inputLinear;

	for (u32 y = 0; y < outputHeight; ++y) {
		for (u32 x = 0; x < outputWidth; ++x) {
			const u32 inX = x << horizontalScale;
			const u32 inY = y << verticalScale;
			const u32 outY = flipVertically ? (outputHeight - 1 - y) : y;

			u32 srcOffset = 0;
			if (sourceTiled) {
				const u32 coarseX = inX & ~7u;
				const u32 coarseY = inY & ~7u;
				srcOffset = ((coarseY * inputWidth) + (coarseX * 8u)) * srcBpp + getMortonOffset8x8(inX, inY, srcBpp);
			} else {
				srcOffset = (inX + inY * inputWidth) * srcBpp;
			}

			u32 dstOffset = 0;
			if (outputTiled) {
				const u32 coarseX = x & ~7u;
				const u32 coarseY = outY & ~7u;
				dstOffset = ((coarseY * outputWidth) + (coarseX * 8u)) * dstBpp + getMortonOffset8x8(x, outY, dstBpp);
			} else {
				dstOffset = (x + outY * outputWidth) * dstBpp;
			}

			const u8* srcPixel = src + srcOffset;
			RGBA c = decodePixel(inputFmt, srcPixel);

			if (scalingMode == 1) {
				const RGBA c1 = decodePixel(inputFmt, srcPixel + srcBpp);
				c = avg2(c, c1);
			} else if (scalingMode == 2) {
				const RGBA c1 = decodePixel(inputFmt, srcPixel + srcBpp);
				const RGBA c2 = decodePixel(inputFmt, srcPixel + srcBpp * inputWidth);
				const RGBA c3 = decodePixel(inputFmt, srcPixel + srcBpp * (inputWidth + 1));
				c = avg4(c, c1, c2, c3);
			}

			encodePixel(outputFmt, c, dst + dstOffset);
		}
	}
}

void RendererSw::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	(void)flags;

	const u32 remaining = totalBytes & ~0xFu;
	if (remaining == 0) {
		return;
	}

	const u32 inputWidth = (inputSize & 0xFFFF) * 16;
	const u32 inputGap = (inputSize >> 16) * 16;
	const u32 outputWidth = (outputSize & 0xFFFF) * 16;
	const u32 outputGap = (outputSize >> 16) * 16;

	const u32 inWidth = (inputGap == 0) ? remaining : inputWidth;
	const u32 outWidth = (outputGap == 0) ? remaining : outputWidth;
	if (inWidth == 0 || outWidth == 0) {
		return;
	}

	doSoftwareTextureCopy(inputAddr, outputAddr, remaining, inWidth, inputGap, outWidth, outputGap);
}

void RendererSw::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	if (!rasterizer || vertices.size() < 3) {
		return;
	}

	if (primType == PICA::PrimType::TriangleList) {
		rasterizer->drawTriangles(colourBufferLoc, colourBufferFormat, depthBufferLoc, depthBufferFormat, fbSize, vertices);
		return;
	}

	std::vector<PICA::Vertex> assembled;
	assembled.reserve(vertices.size() * 3);

	if (primType == PICA::PrimType::TriangleStrip) {
		for (size_t i = 0; i + 2 < vertices.size(); ++i) {
			if ((i & 1) == 0) {
				assembled.push_back(vertices[i + 0]);
				assembled.push_back(vertices[i + 1]);
				assembled.push_back(vertices[i + 2]);
			} else {
				assembled.push_back(vertices[i + 1]);
				assembled.push_back(vertices[i + 0]);
				assembled.push_back(vertices[i + 2]);
			}
		}
	} else if (primType == PICA::PrimType::TriangleFan) {
		const PICA::Vertex center = vertices[0];
		for (size_t i = 1; i + 1 < vertices.size(); ++i) {
			assembled.push_back(center);
			assembled.push_back(vertices[i]);
			assembled.push_back(vertices[i + 1]);
		}
	} else {
		return;
	}

	if (!assembled.empty()) {
		rasterizer->drawTriangles(colourBufferLoc, colourBufferFormat, depthBufferLoc, depthBufferFormat, fbSize, assembled);
	}
}

void RendererSw::screenshot(const std::string& name) {
	const u32 width = fbSize[0];
	const u32 height = fbSize[1];
	if (width == 0 || height == 0) {
		Helpers::warn("RendererSW::screenshot: Framebuffer size is invalid (%u x %u)\n", width, height);
		return;
	}

	const u32 bpp = static_cast<u32>(PICA::sizePerPixel(colourBufferFormat));
	if (bpp == 0) {
		Helpers::warn("RendererSW::screenshot: Unsupported colour format %u\n", static_cast<u32>(colourBufferFormat));
		return;
	}

	const u8* colourBuffer = gpu.getPointerPhys<u8>(colourBufferLoc);
	if (colourBuffer == nullptr) {
		Helpers::warn("RendererSW::screenshot: Colour buffer is not mapped (paddr=%08X)\n", colourBufferLoc);
		return;
	}

	std::filesystem::path screenshotDir = IOFile::getUserData() / "screenshots";
	std::error_code ec;
	std::filesystem::create_directories(screenshotDir, ec);
	if (ec) {
		Helpers::warn("RendererSW::screenshot: Failed to create screenshot directory %s (error: %s)\n",
			screenshotDir.string().c_str(), ec.message().c_str());
		return;
	}

	std::filesystem::path outputPath(name);
	if (outputPath.extension().empty()) {
		outputPath += ".ppm";
	}
	if (!outputPath.is_absolute()) {
		outputPath = screenshotDir / outputPath;
	}

	IOFile out(outputPath, "wb");
	if (!out.isOpen()) {
		Helpers::warn("RendererSW::screenshot: Failed to open output file %s\n", outputPath.string().c_str());
		return;
	}

	const std::string header = "P6\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
	out.writeBytes(header.data(), header.size());

	std::vector<u8> row;
	row.resize(width * 3);

	for (u32 y = 0; y < height; y++) {
		for (u32 x = 0; x < width; x++) {
			const u32 offset = (y * width + x) * bpp;
			const RGBA pixel = decodePixel(colourBufferFormat, colourBuffer + offset);

			const u32 outOffset = x * 3;
			row[outOffset + 0] = pixel.r;
			row[outOffset + 1] = pixel.g;
			row[outOffset + 2] = pixel.b;
		}
		out.writeBytes(row.data(), row.size());
	}

	out.flush();
	out.close();
	Helpers::warn("RendererSW::screenshot: Wrote %s\n", outputPath.string().c_str());
}
void RendererSw::deinitGraphicsContext() {
	// Software renderer does not keep external context state.
}
