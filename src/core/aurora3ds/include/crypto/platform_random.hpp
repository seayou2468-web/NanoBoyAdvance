#pragma once

#include <vector>

#include "../helpers.hpp"

namespace Crypto {

bool GenerateSecureRandomBytes(u8* data, usize size);

inline bool GenerateSecureRandomBytes(std::vector<u8>& out) {
    return GenerateSecureRandomBytes(out.data(), out.size());
}

} // namespace Crypto
