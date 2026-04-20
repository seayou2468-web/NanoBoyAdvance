#include "PICA/pica_hash.hpp"
#include "PICA/shader.hpp"

#define XXH_INLINE_ALL
#include "xxhash/xxhash.h"

PICAHash::HashType PICAHash::computeHash(const char* buf, std::size_t len) {
	return XXH3_64bits(buf, len);
}

PICAShader::Hash PICAShader::getCodeHash() {
	// Hash the code again if the code changed
	if (codeHashDirty) {
		codeHashDirty = false;
		lastCodeHash = PICAHash::computeHash((const char*)&loadedShader[0], loadedShader.size() * sizeof(loadedShader[0]));
	}

	// Return the code hash
	return lastCodeHash;
}

PICAShader::Hash PICAShader::getOpdescHash() {
	// Hash the code again if the operand descriptors changed
	if (opdescHashDirty) {
		opdescHashDirty = false;
		lastOpdescHash = PICAHash::computeHash((const char*)&operandDescriptors[0], operandDescriptors.size() * sizeof(operandDescriptors[0]));
	}

	// Return the code hash
	return lastOpdescHash;
}
