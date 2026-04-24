#include "../../../include/crypto/platform_random.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>

namespace Crypto {

static bool FillFromGetRandom(u8* data, usize size) {
    usize offset = 0;
    while (offset < size) {
        const auto rc = getrandom(data + offset, size - offset, 0);
        if (rc > 0) {
            offset += static_cast<usize>(rc);
            continue;
        }
        if (rc == -1 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static bool FillFromURandom(u8* data, usize size) {
    const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    usize offset = 0;
    while (offset < size) {
        const auto rc = read(fd, data + offset, size - offset);
        if (rc > 0) {
            offset += static_cast<usize>(rc);
            continue;
        }
        if (rc == -1 && errno == EINTR) {
            continue;
        }
        close(fd);
        return false;
    }

    close(fd);
    return true;
}

bool GenerateSecureRandomBytes(u8* data, usize size) {
    if (size == 0) {
        return true;
    }

    // Prefer the kernel CSPRNG API and fall back to /dev/urandom for older environments.
    return FillFromGetRandom(data, size) || FillFromURandom(data, size);
}

} // namespace Crypto
