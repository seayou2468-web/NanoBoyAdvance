#pragma once

namespace AuroraSerialization {

class access {
public:
    template <class Archive, class T>
    static void serialize(Archive& ar, T& value, const unsigned int version) {
        value.serialize(ar, version);
    }
};

template <class Base, class Derived>
constexpr Base& base_object(Derived& derived) noexcept {
    return static_cast<Base&>(derived);
}

template <class Base, class Derived>
constexpr const Base& base_object(const Derived& derived) noexcept {
    return static_cast<const Base&>(derived);
}

} // namespace AuroraSerialization
