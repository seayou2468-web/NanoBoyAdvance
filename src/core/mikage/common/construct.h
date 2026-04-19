#pragma once

/// Allows classes to define save_construct/load_construct methods for serialization.
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

#define BOOST_SERIALIZATION_CONSTRUCT(T)                                                           \
    namespace MikageSerialization {                                                               \
    template <class Archive>                                                                       \
    void save_construct_data(Archive& ar, const T* t, const unsigned int file_version) {          \
        construct_access::save_construct(ar, t, file_version);                                     \
    }                                                                                              \
    template <class Archive>                                                                       \
    void load_construct_data(Archive& ar, T* t, const unsigned int file_version) {                \
        construct_access::load_construct(ar, t, file_version);                                     \
    }                                                                                              \
    }
