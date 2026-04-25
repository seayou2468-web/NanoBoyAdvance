// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace aurora::serialization {

// Compatibility shim used by legacy reference/hle serialization code.
class access {};

struct TypeRegistrationInfo {
    std::string exported_name;
    std::uint32_t version = 0;
    bool has_construct_data = false;
};

inline std::unordered_map<std::string, TypeRegistrationInfo>& GetTypeRegistrationRegistry() {
    static std::unordered_map<std::string, TypeRegistrationInfo> registry;
    return registry;
}

inline std::mutex& GetTypeRegistrationRegistryMutex() {
    static std::mutex mutex;
    return mutex;
}

template <class T>
std::string TypeKey() {
    return typeid(T).name();
}

template <class T>
void RegisterExportedType(std::string exported_name) {
    std::scoped_lock lock{GetTypeRegistrationRegistryMutex()};
    auto& info = GetTypeRegistrationRegistry()[TypeKey<T>()];
    info.exported_name = std::move(exported_name);
}

template <class T>
void RegisterTypeConstructSupport() {
    std::scoped_lock lock{GetTypeRegistrationRegistryMutex()};
    auto& info = GetTypeRegistrationRegistry()[TypeKey<T>()];
    info.has_construct_data = true;
}

template <class T>
void RegisterTypeVersion(std::uint32_t version) {
    std::scoped_lock lock{GetTypeRegistrationRegistryMutex()};
    auto& info = GetTypeRegistrationRegistry()[TypeKey<T>()];
    info.version = version;
}

template <class T>
struct Version {
    static constexpr std::uint32_t value = 0;
};

class binary_object {
public:
    constexpr binary_object(void* data_, std::size_t size_) : data(data_), size(size_) {}

    void* Data() const {
        return data;
    }

    std::size_t Size() const {
        return size;
    }

private:
    void* data;
    std::size_t size;
};

class const_binary_object {
public:
    constexpr const_binary_object(const void* data_, std::size_t size_) : data(data_), size(size_) {}

    const void* Data() const {
        return data;
    }

    std::size_t Size() const {
        return size;
    }

private:
    const void* data;
    std::size_t size;
};

constexpr binary_object make_binary_object(void* data, std::size_t size) {
    return {data, size};
}

constexpr const_binary_object make_binary_object(const void* data, std::size_t size) {
    return {data, size};
}

template <class Archive>
void serialize(Archive& ar, binary_object& value, const unsigned int) {
    if constexpr (Archive::is_loading::value) {
        if constexpr (requires { ar.load_binary(value.Data(), value.Size()); }) {
            ar.load_binary(value.Data(), value.Size());
        } else {
            auto* bytes = static_cast<std::uint8_t*>(value.Data());
            for (std::size_t i = 0; i < value.Size(); ++i) {
                ar & bytes[i];
            }
        }
    } else if constexpr (requires { ar.save_binary(value.Data(), value.Size()); }) {
        ar.save_binary(value.Data(), value.Size());
    } else {
        auto* bytes = static_cast<std::uint8_t*>(value.Data());
        for (std::size_t i = 0; i < value.Size(); ++i) {
            ar & bytes[i];
        }
    }
}

template <class Archive>
void serialize(Archive& ar, const_binary_object& value, const unsigned int) {
    if constexpr (Archive::is_loading::value) {
        return;
    } else if constexpr (requires { ar.save_binary(value.Data(), value.Size()); }) {
        ar.save_binary(value.Data(), value.Size());
    } else {
        const auto* bytes = static_cast<const std::uint8_t*>(value.Data());
        for (std::size_t i = 0; i < value.Size(); ++i) {
            auto byte = bytes[i];
            ar & byte;
        }
    }
}

template <class T>
T& base_object(T& object) {
    return object;
}

template <class Archive, class T>
void serialize(Archive& ar, std::optional<T>& value, const unsigned int) {
    bool has_value = value.has_value();
    ar & has_value;
    if constexpr (Archive::is_loading::value) {
        if (has_value) {
            T loaded{};
            ar & loaded;
            value = std::move(loaded);
        } else {
            value.reset();
        }
    } else if (has_value) {
        auto loaded = *value;
        ar & loaded;
    }
}

template <std::size_t Index = 0, class Archive, class... Ts>
void LoadVariantByIndex(Archive& ar, std::variant<Ts...>& variant, std::size_t index) {
    if constexpr (Index < sizeof...(Ts)) {
        if (Index == index) {
            using Alternative = std::variant_alternative_t<Index, std::variant<Ts...>>;
            Alternative loaded{};
            ar & loaded;
            variant = std::move(loaded);
            return;
        }
        LoadVariantByIndex<Index + 1>(ar, variant, index);
    }
}

template <class Archive, class... Ts>
void serialize(Archive& ar, std::variant<Ts...>& value, const unsigned int) {
    using IndexType = std::uint32_t;
    static_assert(sizeof...(Ts) <= std::numeric_limits<IndexType>::max());

    if constexpr (Archive::is_loading::value) {
        IndexType index{};
        ar & index;
        LoadVariantByIndex(ar, value, static_cast<std::size_t>(index));
    } else {
        auto index = static_cast<IndexType>(value.index());
        ar & index;
        std::visit([&ar](auto& active) { ar & active; }, value);
    }
}

} // namespace aurora::serialization

#define HLE_SERIALIZATION_NAMESPACE_BEGIN namespace aurora::serialization {
#define HLE_SERIALIZATION_NAMESPACE_END }
#define HLE_SERIALIZATION_CONCAT_IMPL(a, b) a##b
#define HLE_SERIALIZATION_CONCAT(a, b) HLE_SERIALIZATION_CONCAT_IMPL(a, b)
#define HLE_CLASS_EXPORT_KEY(T)                                                               \
    namespace {                                                                                 \
    [[maybe_unused]] const bool HLE_SERIALIZATION_CONCAT(_hle_export_reg_, __COUNTER__) = \
        []() {                                                                                  \
            ::aurora::serialization::RegisterExportedType<T>(#T);                    \
            return true;                                                                        \
        }();                                                                                    \
    }
#define HLE_CLASS_EXPORT(T) HLE_CLASS_EXPORT_KEY(T)
#define HLE_CLASS_VERSION(T, V)                               \
    template <>                                                  \
    struct aurora::serialization::Version<T> {         \
        static constexpr std::uint32_t value = static_cast<std::uint32_t>(V); \
    };                                                           \
    namespace {                                                  \
    [[maybe_unused]] const bool HLE_SERIALIZATION_CONCAT(_hle_version_reg_, __COUNTER__) = \
        []() {                                                   \
            ::aurora::serialization::RegisterTypeVersion<T>( \
                static_cast<std::uint32_t>(V));                  \
            return true;                                         \
        }();                                                     \
    }
#define HLE_SERIALIZATION_CONSTRUCT(T)                                                               \
    namespace {                                                                                         \
    [[maybe_unused]] const bool HLE_SERIALIZATION_CONCAT(_hle_construct_reg_, __COUNTER__) =     \
        []() {                                                                                          \
            ::aurora::serialization::RegisterTypeConstructSupport<T>();                       \
            return true;                                                                                 \
        }();                                                                                             \
    }
#define HLE_SERIALIZATION_SPLIT_MEMBER()                                                        \
    template <class Archive>                                                                      \
    void serialize(Archive& ar, const unsigned int file_version) {                                \
        if constexpr (Archive::is_loading::value) {                                               \
            load(ar, file_version);                                                                \
        } else {                                                                                   \
            save(ar, file_version);                                                                \
        }                                                                                          \
    }

namespace aurora {

void ReplaceAllInPlace(std::string& input, std::string_view from, std::string_view to);

std::uint8_t Crc8_07(const void* data, std::size_t size);
std::uint16_t Crc16CcittFalse(const void* data, std::size_t size);
std::uint32_t Crc32IsoHdlc(const void* data, std::size_t size);

} // namespace aurora
