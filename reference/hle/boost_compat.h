// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace HLE::BoostCompat::Serialization {

// Compatibility shim used by legacy reference/hle serialization code.
class access {};

template <class T>
T& base_object(T& object) {
    return object;
}

template <class Archive, class T>
void serialize(Archive& ar, std::optional<T>& value, const unsigned int) {
    bool has_value = value.has_value();
    ar & has_value;
    if constexpr (Archive::is_loading::value) {
        if (has_value) {
            T loaded{};
            ar & loaded;
            value = std::move(loaded);
        } else {
            value.reset();
        }
    } else if (has_value) {
        auto loaded = *value;
        ar & loaded;
    }
}

} // namespace HLE::BoostCompat::Serialization

#define HLE_BOOST_SERIALIZATION_BEGIN namespace HLE::BoostCompat::Serialization {
#define HLE_BOOST_SERIALIZATION_END }

namespace HLE::BoostCompat {

void ReplaceAllInPlace(std::string& input, std::string_view from, std::string_view to);

std::uint8_t Crc8_07(const void* data, std::size_t size);
std::uint16_t Crc16CcittFalse(const void* data, std::size_t size);
std::uint32_t Crc32IsoHdlc(const void* data, std::size_t size);

} // namespace HLE::BoostCompat
