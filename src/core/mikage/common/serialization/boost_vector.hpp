#pragma once

#include <vector>
#include "common/serialization/boost_all_serialization.h"

namespace MikageSerialization {

template <class Archive, class T>
void serialize(Archive& ar, std::vector<T>& value, const unsigned int file_version) {
    split_free(ar, value, file_version);
}

} // namespace MikageSerialization
