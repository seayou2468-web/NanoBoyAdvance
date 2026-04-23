#pragma once

#include <cstddef>

#include "./helpers.hpp"

namespace AppleCrypto {
	static constexpr usize SHA256DigestSize = 32;

	void sha256(const u8* data, usize size, u8* digestOut);
	void aes128CtrXcrypt(const u8* key, const u8* initialCounter, usize offset, u8* data, usize size);
}  // namespace AppleCrypto
