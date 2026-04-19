// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../common/archives.h"
#include "../../common/crc.h"
#include "mii.h"
#include "../../common/serialization/boost_all_serialization.h"

SERIALIZE_EXPORT_IMPL(Mii::MiiData)
SERIALIZE_EXPORT_IMPL(Mii::ChecksummedMiiData)

namespace Mii {
template <class Archive>
void MiiData::serialize(Archive& ar, const unsigned int) {
    ar& MikageSerialization::make_binary_object(this, sizeof(MiiData));
}
SERIALIZE_IMPL(MiiData)

template <class Archive>
void ChecksummedMiiData::serialize(Archive& ar, const unsigned int) {
    ar& MikageSerialization::make_binary_object(this, sizeof(ChecksummedMiiData));
}
SERIALIZE_IMPL(ChecksummedMiiData)

u16 ChecksummedMiiData::CalculateChecksum() {
    // Calculate the checksum of the selected Mii, see https://www.3dbrew.org/wiki/Mii#Checksum
    return Common::Crc16_1021(this, offsetof(ChecksummedMiiData, crc16), 0);
}
} // namespace Mii
