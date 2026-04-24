#include "../../../include/crypto/platform_random.hpp"

#include <openssl/rand.h>

namespace Crypto {

bool GenerateSecureRandomBytes(u8* data, usize size) {
    if (size == 0) {
        return true;
    }

    return RAND_bytes(data, static_cast<int>(size)) == 1;
}

} // namespace Crypto
