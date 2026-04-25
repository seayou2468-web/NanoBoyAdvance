// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/global.h"

namespace Core {

template <>
System& Global<System>() {
    static System system{};
    return system;
}

}  // namespace Core
