#pragma once

#include <new>
#include <type_traits>

namespace Core {

template <typename T>
T*& GlobalStorage() {
    static T* ptr = nullptr;
    return ptr;
}

template <typename T>
void SetGlobal(T& value) {
    GlobalStorage<T>() = &value;
}

template <typename T>
T& Global() {
    if (GlobalStorage<T>()) {
        return *GlobalStorage<T>();
    }
    alignas(T) static unsigned char storage[sizeof(T)];
    return *reinterpret_cast<T*>(storage);
}

} // namespace Core
