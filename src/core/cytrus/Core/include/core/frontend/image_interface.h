// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Frontend {

enum class DDSFormat {
    Invalid,
    RGBA8,
    BC1,
    BC3,
    BC5,
    BC7,
    ASTC4x4,
    ASTC6x6,
    ASTC8x6,
};

/**
 * Utility class that provides image decoding/encoding to the custom texture manager.
 * Can be optionally overriden by frontends to provide a custom implementation.
 */
class ImageInterface {
public:
    virtual ~ImageInterface() = default;

    virtual bool DecodePNG(std::vector<u8>& dst, u32& width, u32& height, std::span<const u8> src);
    virtual bool DecodeDDS(std::vector<u8>& dst, u32& width, u32& height, DDSFormat& format,
                           std::span<const u8> src);
    virtual bool EncodePNG(const std::string& path, u32 width, u32 height, std::span<const u8> src);
};

} // namespace Frontend
