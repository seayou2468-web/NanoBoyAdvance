// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/// Lightweight construct helper used by serialization compatibility paths.
class construct_access {
public:
    template <class Archive, class T>
    static void save_construct(Archive& ar, const T* t, const unsigned int file_version) {
        t->save_construct(ar, file_version);
    }
    template <class Archive, class T>
    static void load_construct(Archive& ar, T* t, const unsigned int file_version) {
        T::load_construct(ar, t, file_version);
    }
};

// Keep compatibility macro available even when Boost serialization headers
// are not present in the build environment.
#define BOOST_SERIALIZATION_CONSTRUCT(T)
