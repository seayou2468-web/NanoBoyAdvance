// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/serialization/serialization_alias.hpp"
#include "common/archives.h"
#include "crc16.hpp"
#include "core/hle/mii.h"

SERIALIZE_EXPORT_IMPL(Mii::MiiData)
SERIALIZE_EXPORT_IMPL(Mii::ChecksummedMiiData)

namespace Mii {
template <class Archive>
void MiiData::serialize(Archive& ar, const unsigned int) {
    ar& Serialization::make_binary_object(this, sizeof(MiiData));
}
SERIALIZE_IMPL(MiiData)

template <class Archive>
void ChecksummedMiiData::serialize(Archive& ar, const unsigned int) {
    ar& Serialization::make_binary_object(this, sizeof(ChecksummedMiiData));
}
SERIALIZE_IMPL(ChecksummedMiiData)

u16 ChecksummedMiiData::CalculateChecksum() {
    // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
    return CRC::crc16_1021_no_reflect(this, offsetof(ChecksummedMiiData, crc16));
}
} // namespace Mii
