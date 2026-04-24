// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version

#pragma once

#include <variant>
#include "common/serialization/serialization_alias.hpp"

namespace Serialization {

template <std::size_t I = 0, class Archive, class... Ts>
void LoadVariantByIndex(Archive& ar, std::variant<Ts...>& value, std::size_t index) {
    if constexpr (I < sizeof...(Ts)) {
        if (index == I) {
            value.template emplace<I>();
            ar & std::get<I>(value);
            return;
        }
        LoadVariantByIndex<I + 1>(ar, value, index);
    }
}

template <class Archive, class... Ts>
void save(Archive& ar, const std::variant<Ts...>& value, const unsigned int) {
    std::size_t index = value.index();
    ar & index;
    std::visit([&ar](const auto& v) { ar & v; }, value);
}

template <class Archive, class... Ts>
void load(Archive& ar, std::variant<Ts...>& value, const unsigned int) {
    std::size_t index = 0;
    ar & index;
    LoadVariantByIndex(ar, value, index);
}

template <class Archive, class... Ts>
void serialize(Archive& ar, std::variant<Ts...>& value, const unsigned int version) {
    split_free(ar, value, version);
}

} // namespace Serialization
