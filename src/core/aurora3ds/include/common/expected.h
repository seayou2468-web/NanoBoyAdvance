#pragma once

#include <utility>
#include <variant>

namespace Common {

template <typename E>
class Unexpected {
public:
    explicit Unexpected(E e) : error_(std::move(e)) {}
    const E& error() const { return error_; }

private:
    E error_;
};

template <typename T, typename E>
class Expected {
public:
    Expected(const T& v) : data_(v) {}
    Expected(T&& v) : data_(std::move(v)) {}
    Expected(Unexpected<E> e) : data_(e.error()) {}

    bool HasValue() const { return std::holds_alternative<T>(data_); }
    bool has_value() const { return HasValue(); }
    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }
    E error() const { return std::get<E>(data_); }

    explicit operator bool() const { return HasValue(); }

private:
    std::variant<T, E> data_;
};

} // namespace Common
