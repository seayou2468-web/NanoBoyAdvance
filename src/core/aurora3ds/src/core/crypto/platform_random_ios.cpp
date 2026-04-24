#include "../../../include/crypto/platform_random.hpp"

#include <Security/Security.h>

namespace Crypto {

bool GenerateSecureRandomBytes(u8* data, usize size) {
    return SecRandomCopyBytes(kSecRandomDefault, size, data) == errSecSuccess;
}

} // namespace Crypto
