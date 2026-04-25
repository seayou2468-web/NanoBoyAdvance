// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// Centralized Boost includes for HLE reference sources.
#include <boost/serialization/access.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/weak_ptr.hpp>

namespace HLE::BoostCompat {

void ReplaceAllInPlace(std::string& input, std::string_view from, std::string_view to);

std::uint8_t Crc8_07(const void* data, std::size_t size);

std::uint16_t Crc16CcittFalse(const void* data, std::size_t size);
std::uint32_t Crc32IsoHdlc(const void* data, std::size_t size);

} // namespace HLE::BoostCompat

namespace boost::serialization {

template <class Archive, class T>
void save(Archive& ar, const std::optional<T>& value, const unsigned int) {
    const bool has_value = value.has_value();
    ar & has_value;
    if (has_value) {
        ar & *value;
    }
}

template <class Archive, class T>
void load(Archive& ar, std::optional<T>& value, const unsigned int) {
    bool has_value = false;
    ar & has_value;
    if (has_value) {
        T loaded{};
        ar & loaded;
        value = std::move(loaded);
    } else {
        value.reset();
    }
}

template <class Archive, class T>
void serialize(Archive& ar, std::optional<T>& value, const unsigned int version) {
    split_free(ar, value, version);
}

} // namespace boost::serialization
