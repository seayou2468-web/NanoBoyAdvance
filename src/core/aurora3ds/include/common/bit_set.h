#pragma once

#include <cstddef>
#include <cstdint>

class BitSet8 {
public:
    class Iterator {
    public:
        Iterator(std::uint8_t value, int bit) : value_(value), bit_(bit) { Advance(); }

        int operator*() const { return bit_; }
        Iterator& operator++() {
            ++bit_;
            Advance();
            return *this;
        }
        bool operator!=(const Iterator& other) const { return bit_ != other.bit_; }

    private:
        void Advance() {
            while (bit_ < 8 && ((value_ & (1u << bit_)) == 0)) {
                ++bit_;
            }
        }

        std::uint8_t value_{};
        int bit_{};
    };

    explicit BitSet8(std::uint8_t value) : m_val(value) {}

    [[nodiscard]] std::size_t Count() const {
        std::size_t count = 0;
        auto v = m_val;
        while (v) {
            count += (v & 1u);
            v >>= 1;
        }
        return count;
    }

    [[nodiscard]] bool operator[](int index) const {
        return index >= 0 && index < 8 ? (m_val & (1u << index)) != 0 : false;
    }

    Iterator begin() const { return Iterator(m_val, 0); }
    Iterator end() const { return Iterator(m_val, 8); }

    std::uint8_t m_val;
};
