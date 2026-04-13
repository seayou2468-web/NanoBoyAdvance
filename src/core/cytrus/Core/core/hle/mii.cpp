// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/binary_object.hpp>
#include <span>
#include "common/archives.h"
#include "core/hle/mii.h"

SERIALIZE_EXPORT_IMPL(Mii::MiiData)
SERIALIZE_EXPORT_IMPL(Mii::ChecksummedMiiData)

namespace Mii {
static u16 Crc16Ccitt(std::span<const u8> bytes) {
    u16 crc = 0;
    for (u8 byte : bytes) {
        crc ^= static_cast<u16>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<u16>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<u16>(crc << 1);
            }
        }
    }
    return crc;
}

template <class Archive>
void MiiData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::make_binary_object(this, sizeof(MiiData));
}
SERIALIZE_IMPL(MiiData)

template <class Archive>
void ChecksummedMiiData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::make_binary_object(this, sizeof(ChecksummedMiiData));
}
SERIALIZE_IMPL(ChecksummedMiiData)

u16 ChecksummedMiiData::CalculateChecksum() {
    // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
    const auto* bytes = reinterpret_cast<const u8*>(this);
    return Crc16Ccitt(std::span<const u8>{bytes, offsetof(ChecksummedMiiData, crc16)});
}
} // namespace Mii
