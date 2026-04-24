#pragma once

#include "common/vector_math.h"

namespace Common::Color {

inline void EncodeRGBA8(const Common::Vec4<u8>& c, u8* out) {
    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
    out[3] = c[3];
}

inline void EncodeRGB8(const Common::Vec4<u8>& c, u8* out) {
    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
}

inline void EncodeRGB5A1(const Common::Vec4<u8>& c, u8* out) {
    const u16 value = static_cast<u16>(((c[0] >> 3) << 11) | ((c[1] >> 3) << 6) |
                                       ((c[2] >> 3) << 1) | (c[3] >> 7));
    out[0] = static_cast<u8>(value & 0xFF);
    out[1] = static_cast<u8>(value >> 8);
}

inline void EncodeRGB565(const Common::Vec4<u8>& c, u8* out) {
    const u16 value = static_cast<u16>(((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3));
    out[0] = static_cast<u8>(value & 0xFF);
    out[1] = static_cast<u8>(value >> 8);
}

} // namespace Common::Color
