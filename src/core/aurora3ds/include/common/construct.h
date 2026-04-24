#pragma once

class construct_access {
public:
    template <class Archive, class T>
    static void save_construct_data(Archive& ar, const T* t, const unsigned int version) {
        ::save_construct_data(ar, t, version);
    }

    template <class Archive, class T>
    static void load_construct_data(Archive& ar, T* t, const unsigned int version) {
        ::load_construct_data(ar, t, version);
    }
};
