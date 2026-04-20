#pragma once
#include <cstdint>
#include <cstddef>

// Defines to pick which hash algorithm to use for the PICA (For hashing shaders, etc)
// Please only define one of them
// Available algorithms:
// xxh3: 64-bit non-cryptographic hash using SIMD, default.

#define PANDA3DS_PICA_XXHASH3

namespace PICAHash {
	using HashType = std::uint64_t;

	HashType computeHash(const char* buf, std::size_t len);
} // namespace PICAHash 
