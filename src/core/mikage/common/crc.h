// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdint>

namespace Common {

inline std::uint8_t Crc8_07(const void* data, std::size_t size, std::uint8_t init = 0) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint8_t crc = init;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint8_t>((crc & 0x80u) ? ((crc << 1) ^ 0x07u) : (crc << 1));
        }
    }
    return crc;
}

inline std::uint16_t Crc16_1021(const void* data, std::size_t size, std::uint16_t init = 0) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint16_t crc = init;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint16_t>(bytes[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1));
        }
    }
    return crc;
}

inline std::uint32_t Crc32_EdB88320(const void* data, std::size_t size,
                                    std::uint32_t init = 0xFFFFFFFFu) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = init;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint32_t>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

} // namespace Common
