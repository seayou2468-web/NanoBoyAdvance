#pragma once

#include <atomic>
#include "boost_all_serialization.h"

namespace MikageSerialization {

template <class Archive, class T>
void serialize(Archive& ar, std::atomic<T>& value, const unsigned int file_version) {
    split_free(ar, value, file_version);
}

template <class Archive, class T>
void save(Archive&, const std::atomic<T>&, const unsigned int) {}

template <class Archive, class T>
void load(Archive&, std::atomic<T>&, const unsigned int) {}

} // namespace MikageSerialization
