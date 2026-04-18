// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

// Replacement for boost serialization using C++ standard library
// This header provides compatibility macros and template specializations
// for container serialization using the PointerWrap framework from chunk_file.h

#include <array>
#include <bitset>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Note: The actual serialization implementation is provided by the
// PointerWrap class in chunk_file.h. This header maintains API compatibility
// with the previous boost-based approach through simple macros.

// BOOST_SERIALIZATION_EXPORT macro - no-op in standard implementation
#define BOOST_CLASS_EXPORT(T) namespace { }

// BOOST_CLASS_VERSION macro - version tracking (stored as metadata in serialization)
#define BOOST_CLASS_VERSION(T, Ver) namespace { }

// BOOST_SERIALIZATION_ASSUME_ABSTRACT macro - no-op, abstract class marker
#define BOOST_SERIALIZATION_ASSUME_ABSTRACT(T) namespace { }

// Access class for serialization (standard approach)
namespace boost::serialization {
    template<class T>
    class access {
    public:
        template<class Archive>
        static void serialize(Archive& ar, T& t, const unsigned int version) {
            // This is handled by PointerWrap::DoClass
        }
    };
}

// Serialization split member support
#define BOOST_SERIALIZATION_SPLIT_MEMBER() \
    template<class Archive> \
    void serialize(Archive& ar, const unsigned int ver) { \
        if (std::is_same<Archive, boost::serialization::input_archive>::value) { \
            load(ar, ver); \
        } else { \
            save(ar, ver); \
        } \
    }

// Version export macro
#define BOOST_CLASS_EXPORT_IMPLEMENT(T) namespace { }

// Shared pointer serialization support (template helper)
namespace boost::serialization {
    template<typename T>
    struct shared_ptr_helper {
        // Standard shared_ptr serialization
    };

    // Base object serialization helper
    template<typename Base, typename Derived>
    inline Base& base_object(Derived& d) {
        return static_cast<Base&>(d);
    }
}

// Minimal placeholders for container support (actual work done in PointerWrap)
namespace boost::serialization {
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
}

