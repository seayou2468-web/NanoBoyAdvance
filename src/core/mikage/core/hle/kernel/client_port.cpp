// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../common/archives.h"
#include "../../../common/assert.h"
#include "../../global.h"
#include "client_port.h"
#include "client_session.h"
#include "errors.h"
#include "hle_ipc.h"
#include "object.h"
#include "server_port.h"
#include "server_session.h"
#include "../../../common/serialization/boost_all_serialization.h"

SERIALIZE_EXPORT_IMPL(Kernel::ClientPort)

namespace Kernel {

ClientPort::ClientPort(KernelSystem& kernel) : Object(kernel), kernel(kernel) {}
ClientPort::~ClientPort() = default;

Result ClientPort::Connect(std::shared_ptr<ClientSession>* out_client_session) {
    // Note: Threads do not wait for the server endpoint to call
    // AcceptSession before returning from this call.

    R_UNLESS(active_sessions < max_sessions, ResultMaxConnectionsReached);
    active_sessions++;

    // Create a new session pair, let the created sessions inherit the parent port's HLE handler.
    auto [server, client] = kernel.CreateSessionPair(server_port->GetName(), SharedFrom(this));

    if (server_port->hle_handler) {
        server_port->hle_handler->ClientConnected(server);
    } else {
        server_port->pending_sessions.push_back(server);
    }

    // Wake the threads waiting on the ServerPort
    server_port->WakeupAllWaitingThreads();

    *out_client_session = client;
    return ResultSuccess;
}

void ClientPort::ConnectionClosed() {
    ASSERT(active_sessions > 0);
    --active_sessions;
}

template <class Archive>
void ClientPort::serialize(Archive& ar, const unsigned int) {
    ar& MikageSerialization::base_object<Object>(*this);
    ar & server_port;
    ar & max_sessions;
    ar & active_sessions;
    ar & name;
}
SERIALIZE_IMPL(ClientPort)

} // namespace Kernel
