#include "../../../include/apple_crypto.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__APPLE__) || !TARGET_OS_IPHONE

#include <algorithm>
#include <array>
#include <vector>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>

namespace AppleCrypto {
	void sha256(const u8* data, usize size, u8* digestOut) {
		CryptoPP::SHA256 hash;
		hash.CalculateDigest(digestOut, data, size);
	}

	void aes128CtrXcrypt(const u8* key, const u8* initialCounter, usize offset, u8* data, usize size) {
		CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption ctr;
		ctr.SetKeyWithIV(key, 16, initialCounter, 16);

		std::array<u8, 64> zeroes {};
		std::array<u8, 64> scratch {};
		while (offset > 0) {
			const usize chunk = std::min<usize>(offset, zeroes.size());
			ctr.ProcessData(scratch.data(), zeroes.data(), chunk);
			offset -= chunk;
		}

		ctr.ProcessData(data, data, size);
	}
}  // namespace AppleCrypto

#endif
