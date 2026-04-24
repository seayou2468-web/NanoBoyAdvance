#pragma once

#include <cstdint>
#include <type_traits>

namespace Common {

template <int Position, int Bits, typename Storage>
class BitField {
    static_assert(std::is_integral_v<Storage>);
    static constexpr Storage Mask = (Bits >= int(sizeof(Storage) * 8))
                                        ? static_cast<Storage>(~Storage{0})
                                        : static_cast<Storage>((Storage{1} << Bits) - 1);

public:
    constexpr BitField() = default;
    constexpr explicit BitField(Storage& backing) : value_ref(backing) {}

    constexpr Storage Value() const {
        return (value_ref >> Position) & Mask;
    }

    constexpr void Assign(Storage v) {
        value_ref = static_cast<Storage>((value_ref & ~(Mask << Position)) | ((v & Mask) << Position));
    }

    constexpr operator Storage() const {
        return Value();
    }

private:
    Storage& value_ref = dummy;
    inline static Storage dummy{};
};

} // namespace Common
