// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#include "boost_compat.h"

namespace aurora {

namespace {

template <typename OutT>
OutT CrcNoReflect(const std::uint8_t* bytes, std::size_t size, OutT polynomial, OutT init,
                  OutT xor_out) {
    constexpr int bit_width = static_cast<int>(sizeof(OutT) * 8);
    const OutT top_bit = static_cast<OutT>(OutT{1} << (bit_width - 1));

    OutT crc = init;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<OutT>(static_cast<OutT>(bytes[i]) << (bit_width - 8));
        for (int bit = 0; bit < 8; ++bit) {
            const bool carry = (crc & top_bit) != 0;
            crc = static_cast<OutT>(crc << 1);
            if (carry) {
                crc ^= polynomial;
            }
        }
    }
    return static_cast<OutT>(crc ^ xor_out);
}

std::uint32_t Crc32Reflect(const std::uint8_t* bytes, std::size_t size, std::uint32_t polynomial,
                           std::uint32_t init, std::uint32_t xor_out) {
    std::uint32_t crc = init;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const bool carry = (crc & 1u) != 0;
            crc >>= 1;
            if (carry) {
                crc ^= polynomial;
            }
        }
    }
    return crc ^ xor_out;
}

} // namespace

std::uint8_t Crc8_07(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return CrcNoReflect<std::uint8_t>(bytes, size, 0x07u, 0x00u, 0x00u);
}

std::uint16_t Crc16CcittFalse(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return CrcNoReflect<std::uint16_t>(bytes, size, 0x1021u, 0x0000u, 0x0000u);
}

std::uint32_t Crc32IsoHdlc(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    return Crc32Reflect(bytes, size, 0xEDB88320u, 0xFFFFFFFFu, 0xFFFFFFFFu);
}

} // namespace aurora
