#include "../../../include/services/pm.hpp"

#include "../../../include/ipc.hpp"

void PMService::reset() {}

void PMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	log("pm:%s command %08X (stubbed)\n", type == Type::App ? "app" : "dbg", command);

	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
