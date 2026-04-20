#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "helpers.hpp"

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#endif

namespace AppleCrypto {
	static constexpr usize SHA256DigestSize = 32;

	static inline void sha256(const u8* data, usize size, u8* digestOut) {
#if defined(__APPLE__)
		CC_SHA256(data, static_cast<CC_LONG>(size), digestOut);
#else
		Helpers::panic("AppleCrypto::sha256 requires Apple CommonCrypto");
#endif
	}

	static inline void aes128CtrXcrypt(const u8* key, const u8* initialCounter, usize offset, u8* data, usize size) {
#if defined(__APPLE__)
		CCCryptorRef cryptor = nullptr;
		const CCCryptorStatus createStatus = CCCryptorCreateWithMode(
			kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, ccNoPadding, initialCounter, key, 16, nullptr, 0, 0, kCCModeOptionCTR_BE, &cryptor
		);

		if (createStatus != kCCSuccess || cryptor == nullptr) {
			Helpers::panic("Failed to initialize CommonCrypto CTR context");
		}

		std::array<u8, 64> zeroes {};
		std::array<u8, 64> scratch {};
		while (offset > 0) {
			const usize chunk = std::min<usize>(offset, zeroes.size());
			size_t moved = 0;
			const CCCryptorStatus status = CCCryptorUpdate(cryptor, zeroes.data(), chunk, scratch.data(), scratch.size(), &moved);
			if (status != kCCSuccess || moved != chunk) {
				CCCryptorRelease(cryptor);
				Helpers::panic("Failed to seek CommonCrypto CTR stream");
			}
			offset -= chunk;
		}

		size_t moved = 0;
		const CCCryptorStatus status = CCCryptorUpdate(cryptor, data, size, data, size, &moved);
		CCCryptorRelease(cryptor);

		if (status != kCCSuccess || moved != size) {
			Helpers::panic("Failed to process CommonCrypto CTR stream");
		}
#else
		Helpers::panic("AppleCrypto::aes128CtrXcrypt requires Apple CommonCrypto");
#endif
	}
}  // namespace AppleCrypto
