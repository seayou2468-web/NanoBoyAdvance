#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace boost::serialization {

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

template <class Archive, class T>
void split_member(Archive& ar, T& obj, const unsigned int version) {
    if constexpr (Archive::is_saving::value) {
        obj.save(ar, version);
    } else {
        obj.load(ar, version);
    }
}

} // namespace boost::serialization

#define BOOST_CLASS_EXPORT_KEY(...)
#define BOOST_CLASS_EXPORT(...)
#define BOOST_SERIALIZATION_SPLIT_MEMBER()                                                     \
    template <class Archive>                                                                   \
    void serialize(Archive& ar, const unsigned int version) {                                 \
        ::boost::serialization::split_member(ar, *this, version);                             \
    }
