#include "../../../include/services/pm.hpp"

#include "../../../include/ipc.hpp"

namespace PMCommands {
	enum : u32 {
		SetAppResourceLimit = 0x000A,
		GetAppResourceLimit = 0x000B,
	};
}

void PMService::reset() {
	appCpuTimeLimit = 30;
}

void PMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	if (type == Type::App) {
		switch (commandID) {
			case PMCommands::SetAppResourceLimit: setAppResourceLimit(messagePointer); return;
			case PMCommands::GetAppResourceLimit: getAppResourceLimit(messagePointer); return;
			default: break;
		}
	}

	log("pm:%s command %08X [default-success]\n", type == Type::App ? "app" : "dbg", command);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::setAppResourceLimit(u32 messagePointer) {
	constexpr u32 CpuTimeResourceType = 0;

	const u32 resourceType = mem.read32(messagePointer + 8);
	const u32 value = mem.read32(messagePointer + 12);
	log("pm:app::SetAppResourceLimit(type=%u, value=%u)\n", resourceType, value);

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::SetAppResourceLimit, 1, 0));
	if (resourceType != CpuTimeResourceType) {
		mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		return;
	}

	appCpuTimeLimit = value;
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::getAppResourceLimit(u32 messagePointer) {
	constexpr u32 CpuTimeResourceType = 0;

	const u32 resourceType = mem.read32(messagePointer + 8);
	log("pm:app::GetAppResourceLimit(type=%u)\n", resourceType);

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::GetAppResourceLimit, 3, 0));
	if (resourceType != CpuTimeResourceType) {
		mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		mem.write64(messagePointer + 8, 0);
		return;
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, static_cast<u64>(appCpuTimeLimit));
}
