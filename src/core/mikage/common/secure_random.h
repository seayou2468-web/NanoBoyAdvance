// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <type_traits>

#if defined(__APPLE__)
#include <Security/Security.h>
#endif

namespace Common {

inline bool FillSecureRandom(std::span<std::uint8_t> out) {
    if (out.empty()) {
        return true;
    }

#if defined(__APPLE__)
    return SecRandomCopyBytes(kSecRandomDefault, out.size(), out.data()) == errSecSuccess;
#else
    std::random_device device;
    for (auto& byte : out) {
        byte = static_cast<std::uint8_t>(device() & 0xFFu);
    }
    return true;
#endif
}

template <std::size_t N>
inline bool FillSecureRandom(std::array<std::uint8_t, N>& out) {
    return FillSecureRandom(std::span<std::uint8_t>(out.data(), out.size()));
}

template <typename T>
inline bool FillSecureRandomValue(T& out) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto* data = reinterpret_cast<std::uint8_t*>(&out);
    return FillSecureRandom(std::span<std::uint8_t>(data, sizeof(T)));
}

} // namespace Common
