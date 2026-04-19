// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../common/archives.h"
#include "client_port.h"
#include "client_session.h"
#include "server_session.h"
#include "session.h"
#include "../../../common/serialization/boost_all_serialization.h"

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
