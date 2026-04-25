// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

namespace boost::serialization {

template <class Archive, class T>
void serialize(Archive& ar, std::atomic<T>& value, const unsigned int /*file_version*/) {
    T tmp;
    if constexpr (Archive::is_saving::value) {
        tmp = value.load();
    }
    ar& tmp;
    value.store(tmp);
}

}  // namespace boost::serialization
