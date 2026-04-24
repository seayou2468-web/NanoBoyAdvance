#pragma once

#include <type_traits>

#include "common/common_types.h"

namespace Common {

template <int Position, int Bits, typename T, typename StorageType = u32>
class BitField {
    static_assert(std::is_integral_v<StorageType>, "BitField storage must be integral");
    template <typename U, bool IsEnum = std::is_enum_v<U>>
    struct RawTypeHelper {
        using type = U;
    };
    template <typename U>
    struct RawTypeHelper<U, true> {
        using type = std::underlying_type_t<U>;
    };
    using RawT = typename RawTypeHelper<T>::type;

    static constexpr StorageType mask =
        Bits >= static_cast<int>(sizeof(StorageType) * 8)
            ? static_cast<StorageType>(~StorageType{0})
            : static_cast<StorageType>((StorageType{1} << Bits) - 1);

public:
    constexpr BitField() = default;

    constexpr T ExtractValue(StorageType storage) const {
        const StorageType value = static_cast<StorageType>((storage >> Position) & mask);
        if constexpr (std::is_enum_v<T>) {
            return static_cast<T>(value);
        } else {
            return static_cast<T>(value);
        }
    }

    constexpr StorageType FormatValue(T value) const {
        const StorageType raw = static_cast<StorageType>(static_cast<RawT>(value));
        return static_cast<StorageType>((raw & mask) << Position);
    }

    constexpr void Assign(StorageType& storage, T value) const {
        storage = static_cast<StorageType>((storage & ~(mask << Position)) | FormatValue(value));
    }
};

} // namespace Common

template <int Position, int Bits, typename T, typename StorageType = u32>
using BitField = Common::BitField<Position, Bits, T, StorageType>;
