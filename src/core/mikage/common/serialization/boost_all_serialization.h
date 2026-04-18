// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#define BOOST_CLASS_EXPORT(T) namespace { }
#define BOOST_CLASS_EXPORT_KEY(T) namespace { }
#define BOOST_SERIALIZATION_REGISTER_ARCHIVE(T) namespace { }
#define BOOST_CLASS_VERSION(T, Ver) namespace { }
#define BOOST_SERIALIZATION_ASSUME_ABSTRACT(T) namespace { }
#define BOOST_CLASS_EXPORT_IMPLEMENT(T) namespace { }

namespace MikageSerialization {

struct input_archive {};
struct output_archive {};

template <class T>
class access {
public:
    template <class Archive>
    static void serialize(Archive&, T&, const unsigned int) {}
};

template <class Archive, class T>
inline void split_free(Archive&, T&, const unsigned int) {}

template <typename T>
struct shared_ptr_helper {};

template <typename Base, typename Derived>
inline Base& base_object(Derived& d) {
    return static_cast<Base&>(d);
}

template <typename T>
struct binary_object_ref {
    const T* ptr;
    std::size_t size;
};

template <typename T>
inline binary_object_ref<T> make_binary_object(const T* ptr, std::size_t size) {
    return {ptr, size};
}

struct array_tag {};
struct bitset_tag {};
struct deque_tag {};
struct list_tag {};
struct map_tag {};
struct optional_tag {};
struct set_tag {};
struct string_tag {};
struct unordered_map_tag {};
struct vector_tag {};
struct weak_ptr_tag {};

} // namespace MikageSerialization

namespace boost {
namespace serialization = MikageSerialization;
}

#define BOOST_SERIALIZATION_SPLIT_MEMBER()                                                         \
    template <class Archive>                                                                       \
    void serialize(Archive& ar, const unsigned int ver) {                                          \
        if (std::is_same<Archive, MikageSerialization::input_archive>::value) {                    \
            load(ar, ver);                                                                          \
        } else {                                                                                    \
            save(ar, ver);                                                                          \
        }                                                                                           \
    }
