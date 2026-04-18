#pragma once

#include <variant>
#include "common/serialization/boost_all_serialization.h"

namespace MikageSerialization {

template <class Archive, class... Types>
void serialize(Archive& ar, std::variant<Types...>& value, const unsigned int file_version) {
    split_free(ar, value, file_version);
}

} // namespace MikageSerialization
