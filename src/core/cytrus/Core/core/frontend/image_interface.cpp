// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/frontend/image_interface.h"

namespace Frontend {

namespace {

constexpr u32 DDSMagic = 0x20534444; // "DDS "
constexpr u32 FourCCDXT1 = 0x31545844;
constexpr u32 FourCCDXT3 = 0x33545844;
constexpr u32 FourCCDXT5 = 0x35545844;
constexpr u32 FourCCATI2 = 0x32495441;
constexpr u32 FourCCBC5U = 0x55354342;
constexpr u32 FourCCDX10 = 0x30315844;

struct DDSHeader {
    u32 size;
    u32 flags;
    u32 height;
    u32 width;
    u32 pitch_or_linear_size;
    u32 depth;
    u32 mip_map_count;
    std::array<u32, 11> reserved1;
    struct {
        u32 size;
        u32 flags;
        u32 four_cc;
        u32 rgb_bit_count;
        u32 r_mask;
        u32 g_mask;
        u32 b_mask;
        u32 a_mask;
    } ddspf;
    u32 caps;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};

bool ReadPNGData(std::vector<u8>& dst, u32& width, u32& height, std::span<const u8> src) {
    CFDataRef input_data = CFDataCreate(kCFAllocatorDefault, src.data(), src.size());
    if (!input_data) {
        return false;
    }
    CGImageSourceRef source = CGImageSourceCreateWithData(input_data, nullptr);
    CFRelease(input_data);
    if (!source) {
        return false;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!image) {
        return false;
    }

    width = static_cast<u32>(CGImageGetWidth(image));
    height = static_cast<u32>(CGImageGetHeight(image));
    dst.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx =
        CGBitmapContextCreate(dst.data(), width, height, 8, width * 4, color_space,
                              kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(color_space);
    if (!ctx) {
        CGImageRelease(image);
        return false;
    }
    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
    CGContextRelease(ctx);
    CGImageRelease(image);
    return true;
}

} // namespace

bool ImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                               std::span<const u8> src) {
    if (!ReadPNGData(dst, width, height, src)) {
        LOG_ERROR(Frontend, "Failed to decode PNG with ImageIO");
        return false;
    }
    return true;
}

bool ImageInterface::DecodeDDS(std::vector<u8>& dst, u32& width, u32& height, DDSFormat& format,
                               std::span<const u8> src) {
    if (src.size() < sizeof(u32) + sizeof(DDSHeader)) {
        LOG_ERROR(Frontend, "DDS decode failed: input too small");
        return false;
    }

    u32 magic{};
    std::memcpy(&magic, src.data(), sizeof(magic));
    if (magic != DDSMagic) {
        LOG_ERROR(Frontend, "DDS decode failed: bad magic");
        return false;
    }

    const auto* header = reinterpret_cast<const DDSHeader*>(src.data() + sizeof(u32));
    width = header->width;
    height = header->height;
    format = DDSFormat::Invalid;

    if (header->ddspf.four_cc == FourCCDXT1) {
        format = DDSFormat::BC1;
    } else if (header->ddspf.four_cc == FourCCDXT3 || header->ddspf.four_cc == FourCCDXT5) {
        format = DDSFormat::BC3;
    } else if (header->ddspf.four_cc == FourCCATI2 || header->ddspf.four_cc == FourCCBC5U) {
        format = DDSFormat::BC5;
    } else if (header->ddspf.four_cc == FourCCDX10) {
        LOG_ERROR(Frontend, "DDS decode failed: DX10 extended header not supported");
        return false;
    } else if (header->ddspf.four_cc == 0 && header->ddspf.rgb_bit_count == 32) {
        format = DDSFormat::RGBA8;
    }

    if (format == DDSFormat::Invalid) {
        LOG_ERROR(Frontend, "DDS decode failed: unsupported format");
        return false;
    }

    constexpr std::size_t header_size = sizeof(u32) + sizeof(DDSHeader);
    const auto payload_size = src.size() - header_size;
    dst.resize(payload_size);
    std::memcpy(dst.data(), src.data() + header_size, payload_size);
    return true;
}

bool ImageInterface::EncodePNG(const std::string& path, u32 width, u32 height,
                               std::span<const u8> src) {
    CFMutableDataRef output_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (!output_data) {
        return false;
    }

    CGDataProviderRef provider =
        CGDataProviderCreateWithData(nullptr, src.data(), src.size(), nullptr);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width, height, 8, 32, width * 4, color_space,
                                     kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big,
                                     provider, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
    if (!image) {
        CFRelease(output_data);
        return false;
    }

    CGImageDestinationRef destination =
        CGImageDestinationCreateWithData(output_data, CFSTR("public.png"), 1, nullptr);
    if (!destination) {
        CGImageRelease(image);
        CFRelease(output_data);
        return false;
    }
    CGImageDestinationAddImage(destination, image, nullptr);
    const bool ok = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);
    if (!ok) {
        CFRelease(output_data);
        LOG_ERROR(Frontend, "Failed to encode PNG with ImageIO");
        return false;
    }

    const auto* bytes = CFDataGetBytePtr(output_data);
    const auto size = static_cast<std::size_t>(CFDataGetLength(output_data));
    FileUtil::IOFile file{path, "wb"};
    const bool wrote = file.WriteBytes(bytes, size) == size;
    CFRelease(output_data);
    if (!wrote) {
        LOG_ERROR(Frontend, "Failed to save encoded PNG to path {}", path);
        return false;
    }
    return true;
}

} // namespace Frontend
