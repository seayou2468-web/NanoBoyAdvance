// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/shared_ptr.hpp>
#include "../../../include/common/archives.h"
#include "../../../include/core/hle/kernel/client_port.h"
#include "../../../include/core/hle/kernel/client_session.h"
#include "../../../include/core/hle/kernel/server_session.h"
#include "../../../include/core/hle/kernel/session.h"
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
