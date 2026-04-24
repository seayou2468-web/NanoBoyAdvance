// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version

#pragma once

#include <cstddef>

namespace Serialization {

class access {
public:
    template <class Archive, class T>
    static void serialize(Archive& ar, T& obj, const unsigned int version) {
        obj.serialize(ar, version);
    }
};

template <class Base, class Derived>
Base& base_object(Derived& obj) {
    return static_cast<Base&>(obj);
}

template <class Base, class Derived>
const Base& base_object(const Derived& obj) {
    return static_cast<const Base&>(obj);
}

struct binary_object {
    void* address;
    std::size_t size;
};

struct const_binary_object {
    const void* address;
    std::size_t size;
};

inline binary_object make_binary_object(void* address, std::size_t size) {
    return {address, size};
}

inline const_binary_object make_binary_object(const void* address, std::size_t size) {
    return {address, size};
}

template <class Archive, class T>
void split_member(Archive& ar, T& obj, const unsigned int version) {
    if constexpr (Archive::is_saving::value) {
        obj.save(ar, version);
    } else {
        obj.load(ar, version);
    }
}

template <class Archive, class T>
void split_free(Archive& ar, T& obj, const unsigned int version) {
    if constexpr (Archive::is_saving::value) {
        save(ar, obj, version);
    } else {
        load(ar, obj, version);
    }
}

} // namespace Serialization

#define SERIALIZATION_CLASS_EXPORT_KEY(...)
#define SERIALIZATION_CLASS_EXPORT_IMPL(...)
#define SERIALIZATION_CLASS_EXPORT(...)
#define SERIALIZATION_ASSUME_ABSTRACT(...)
#define SERIALIZATION_CLASS_VERSION(...)
#define SERIALIZATION_CONSTRUCT(T) SERVICE_CONSTRUCT(T)
#define DEBUG_SERIALIZATION_POINT ((void)0)
#define SERIALIZE_IMPL(...)
#define SERIALIZE_EXPORT_IMPL(...)
#define SERVICE_CONSTRUCT_IMPL(...)
#define SERIALIZATION_SPLIT_MEMBER()                                                         \
    template <class Archive>                                                                 \
    void serialize(Archive& ar, const unsigned int version) {                                \
        Serialization::split_member(ar, *this, version);                                     \
    }
