#pragma once

#include <vector>
#include "common/serialization/boost_all_serialization.h"

namespace boost::container {
template <typename T>
using vector = std::vector<T>;
}

namespace boost::serialization {

template <class Archive, class T>
void serialize(Archive& ar, boost::container::vector<T>& value, const unsigned int file_version) {
    split_free(ar, value, file_version);
}

} // namespace boost::serialization
