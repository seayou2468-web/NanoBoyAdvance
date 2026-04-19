#pragma once

#include <set>
#include "../common_types.h"
#include "boost_all_serialization.h"

namespace MikageSerialization {

template <class Archive, class T>
void serialize(Archive& ar, std::set<T>& set, const unsigned int file_version) {
    split_free(ar, set, file_version);
}

} // namespace MikageSerialization
