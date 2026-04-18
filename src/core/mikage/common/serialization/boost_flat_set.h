#pragma once

#include <set>
#include "common/common_types.h"
#include "common/serialization/boost_all_serialization.h"

namespace boost::container {
template <typename T>
using flat_set = std::set<T>;
}

namespace boost::serialization {

template <class Archive, class T>
void serialize(Archive& ar, boost::container::flat_set<T>& set, const unsigned int file_version) {
    split_free(ar, set, file_version);
}

} // namespace boost::serialization
