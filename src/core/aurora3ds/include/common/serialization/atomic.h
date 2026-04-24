#pragma once

#include <atomic>

namespace Serialization {

template <class Archive, class T>
void serialize(Archive& ar, std::atomic<T>& value, const unsigned int) {
    T temp = value.load();
    ar & temp;
    value.store(temp);
}

} // namespace Serialization
