// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <utility>
#include "common/serialization/serialization_alias.hpp"

namespace Serialization {

template <class Archive, class T>
void save(Archive& ar, const std::optional<T>& value, const unsigned int) {
    bool has_value = value.has_value();
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
        value.emplace();
        ar & *value;
    } else {
        value.reset();
    }
}

template <class Archive, class T>
void serialize(Archive& ar, std::optional<T>& value, const unsigned int version) {
    split_free(ar, value, version);
}

} // namespace Serialization
