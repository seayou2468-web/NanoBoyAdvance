#include <cstring>

#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

HorizonHandle Kernel::makePort(const char* name) {
	Handle ret = makeObject(KernelObjectType::Port);
	portHandles.push_back(ret);  // Push the port handle to our cache of port handles
	objects[ret].data = new Port(name);

	return ret;
}

HorizonHandle Kernel::makeSession(Handle portHandle) {
	const auto port = getObject(portHandle, KernelObjectType::Port);
	if (port == nullptr) [[unlikely]] {
		Helpers::panic("Trying to make session for non-existent port");
	}

	// Allocate data for session
	const Handle ret = makeObject(KernelObjectType::Session);
	objects[ret].data = new Session(portHandle);
	return ret;
}

HorizonHandle Kernel::makeNamedPort(const char* name) {
	return makePort(name);
}

HorizonHandle Kernel::makePortSession(Handle portHandle) {
	return makeSession(portHandle);
}

// Get the handle of a port based on its name
// If there's no such port, return nullopt
std::optional<HorizonHandle> Kernel::getPortHandle(const char* name) {
	for (auto handle : portHandles) {
		const auto data = objects[handle].getData<Port>();
		if (std::strncmp(name, data->name, Port::maxNameLen) == 0) {
			return handle;
		}
	}

	return std::nullopt;
}

// Result ConnectToPort(Handle* out, const char* portName)
void Kernel::connectToPort() {
	const u32 handlePointer = regs[0];
	// Read up to max + 1 characters to see if the name is too long
	std::string port = mem.readString(regs[1], Port::maxNameLen + 1);
	logSVC("ConnectToPort(handle pointer = %X, port = \"%s\")\n", handlePointer, port.c_str());

	if (port.size() > Port::maxNameLen) {
		Helpers::warn("ConnectToPort: Port name too long");
		regs[0] = Result::OS::PortNameTooLong;
		return;
	}

	// Try getting a handle to the port
	std::optional<Handle> optionalHandle = getPortHandle(port.c_str());
	// Recover from edge cases where known bootstrap ports were not discoverable by name.
	if (!optionalHandle.has_value()) {
		if (port == "srv:" && getObject(srvHandle, KernelObjectType::Port) != nullptr) {
			optionalHandle = srvHandle;
		} else if (port == "err:f" && getObject(errorPortHandle, KernelObjectType::Port) != nullptr) {
			optionalHandle = errorPortHandle;
		}
	}

	if (!optionalHandle.has_value()) [[unlikely]] {
		// Allow boot to continue for unknown-but-service-like named ports.
		// These ports are routed through OS_STUB in SendSyncRequest.
		const bool looksLikeServicePort = (port.find(':') != std::string::npos) || (!port.empty() && (port[0] == '$' || port[0] == '_'));
		if (looksLikeServicePort && port.length() <= 8) {
			Helpers::warn("ConnectToPort: Auto-creating fallback named port (%s)", port.c_str());
			optionalHandle = makeNamedPort(port.c_str());
		} else {
			Helpers::warn("ConnectToPort: Port doesn't exist (%s)", port.c_str());
			regs[0] = Result::Kernel::NotFound;
			return;
		}
	}

	Handle portHandle = optionalHandle.value();

	const auto portData = objects[portHandle].getData<Port>();
	if (!portData->isPublic) {
		Helpers::warn("ConnectToPort: Attempted to connect to private port");
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	// TODO: Actually create session
	Handle sessionHandle = makeSession(portHandle);

	regs[0] = Result::Success;
	regs[1] = sessionHandle;
}

// Result CreatePort(Handle* server_port, Handle* client_port, const char* name, s32 max_sessions)
void Kernel::createPort() {
	const u32 namePointer = regs[0];
	const s32 maxSessions = static_cast<s32>(regs[1]);
	std::string name;

	if (namePointer != 0) {
		name = mem.readString(namePointer, Port::maxNameLen + 1);
		if (name.size() > Port::maxNameLen) {
			regs[0] = Result::OS::PortNameTooLong;
			return;
		}
	}

	logSVC("CreatePort(name = \"%s\", max sessions = %d)\n", name.c_str(), maxSessions);

	if (maxSessions < 0) {
		regs[0] = Result::OS::OutOfRange;
		return;
	}

	// Aurora currently models a single user-visible port object. Return it for both sides for
	// compatibility and create sessions from this port via CreateSessionToPort/AcceptSession.
	const Handle portHandle = makePort(name.c_str());
	regs[0] = Result::Success;
	regs[1] = portHandle;  // server port
	regs[2] = portHandle;  // client port
}

// Result CreateSessionToPort(Handle* out_client_session, Handle client_port_handle)
void Kernel::createSessionToPort() {
	const Handle portHandle = regs[1];
	logSVC("CreateSessionToPort(port = %X)\n", portHandle);

	const auto port = getObject(portHandle, KernelObjectType::Port);
	if (port == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	regs[0] = Result::Success;
	regs[1] = makeSession(portHandle);
}

// Result CreateSession(Handle* server_session, Handle* client_session)
void Kernel::createSession() {
	logSVC("CreateSession()\n");

	// Aurora currently uses the same session object model for both ends.
	// Use an anonymous private port as backing transport.
	const Handle privatePort = makePort("");
	regs[0] = Result::Success;
	regs[1] = makeSession(privatePort);  // server session
	regs[2] = makeSession(privatePort);  // client session
}

// Result AcceptSession(Handle* out_server_session, Handle server_port_handle)
void Kernel::acceptSession() {
	const Handle portHandle = regs[1];
	logSVC("AcceptSession(port = %X)\n", portHandle);

	const auto port = getObject(portHandle, KernelObjectType::Port);
	if (port == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	regs[0] = Result::Success;
	regs[1] = makeSession(portHandle);
}

// Result SendSyncRequest(Handle session)
// Send an IPC message to a port (typically "srv:") or a service
void Kernel::sendSyncRequest() {
	const auto handle = regs[0];
	u32 messagePointer = getTLSPointer() + 0x80;  // The message is stored starting at TLS+0x80
	logSVC("SendSyncRequest(session handle = %X)\n", handle);

	// Service calls via SendSyncRequest and file access needs to put the caller to sleep for a given amount of time
	// To make sure that the other threads don't get starved. Various games rely on this (including Sonic Boom: Shattering Crystal it seems)
	constexpr u64 syncRequestDelayNs = 39000;
	sleepThread(syncRequestDelayNs);

	// The sync request is being sent at a service rather than whatever port, so have the service manager intercept it
	if (KernelHandles::isServiceHandle(handle)) {
		// The service call might cause a reschedule and change threads. Hence, set r0 before executing the service call
		// Because if the service call goes first, we might corrupt the new thread's r0!!
		regs[0] = Result::Success;
		serviceManager.sendCommandToService(messagePointer, handle);
		return;
	}

	// Check if our sync request is targetting a file instead of a service
	bool isFileOperation = getObject(handle, KernelObjectType::File) != nullptr;
	if (isFileOperation) {
		regs[0] = Result::Success;  // r0 goes first here too
		handleFileOperation(messagePointer, handle);
		return;
	}

	// Check if our sync request is targetting a directory instead of a service
	bool isDirectoryOperation = getObject(handle, KernelObjectType::Directory) != nullptr;
	if (isDirectoryOperation) {
		regs[0] = Result::Success;  // r0 goes first here too
		handleDirectoryOperation(messagePointer, handle);
		return;
	}

	// If we're actually communicating with a port
	const auto session = getObject(handle, KernelObjectType::Session);
	if (session == nullptr) [[unlikely]] {
		Helpers::warn("SendSyncRequest: Invalid handle %X (stubbed response)", handle);
		const u32 command = mem.read32(messagePointer);
		mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
		mem.write32(messagePointer + 4, Result::Success);
		regs[0] = Result::Success;
		return;
	}

	const auto sessionData = static_cast<Session*>(session->data);
	const Handle portHandle = sessionData->portHandle;

	if (portHandle == srvHandle) {  // Special-case SendSyncRequest targetting the "srv: port"
		regs[0] = Result::Success;
		serviceManager.handleSyncRequest(messagePointer);
	} else if (portHandle == errorPortHandle) {  // Special-case "err:f" for juicy logs too
		regs[0] = Result::Success;
		handleErrorSyncRequest(messagePointer);
	} else {
		const auto portData = objects[portHandle].getData<Port>();
		Helpers::warn("SendSyncRequest targetting unsupported port %s", portData->name);
		regs[0] = Result::Success;

		if (std::strchr(portData->name, ':') != nullptr) {
			serviceManager.sendCommandToService(messagePointer, KernelHandles::OS_STUB);
		} else {
			const u32 command = mem.read32(messagePointer);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		}
	}
}
