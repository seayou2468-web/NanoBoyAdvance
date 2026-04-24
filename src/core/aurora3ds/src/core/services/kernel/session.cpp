// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/serialization/serialization_alias.hpp"
#include "common/archives.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"

SERIALIZE_EXPORT_IMPL(Kernel::Session)

namespace Kernel {

template <class Archive>
void Session::serialize(Archive& ar, const unsigned int file_version) {
    ar & client;
    ar & server;
    ar & port;
}
SERIALIZE_IMPL(Session)

} // namespace Kernel
