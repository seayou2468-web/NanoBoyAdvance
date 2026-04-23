#include "../../../include/services/nim.hpp"

#include "../../../include/ipc.hpp"

namespace NIMCommandID {
	enum : u32 {
		StartNetworkUpdate = 0x0001,
		GetProgress = 0x0002,
		GetBackgroundEventForMenu = 0x0005,
		GetBackgroundEventForNews = 0x0006,
		GetNumAutoTitleDownloadTasks = 0x0016,
		GetAutoTitleDownloadTaskInfos = 0x0017,
		GetTslXmlSize = 0x001F,
		Initialize = 0x0021,
		GetDtlXmlSize = 0x0023,
		GetTitleDownloadProgress = 0x0028,
		IsSystemUpdateAvailable = 0x002A,
		StartDownload = 0x0042,
	};
}

void NIMService::reset() {}

void NIMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16; // Lower bits encode IPC descriptors.

	switch (commandID) {
		case NIMCommandID::StartNetworkUpdate: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetProgress: getProgress(messagePointer); break;
		case 0x0003: commandStub(messagePointer, commandID); break;
		case 0x0004: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetBackgroundEventForMenu: commandStubWithHandle(messagePointer, commandID, 0); break;
		case NIMCommandID::GetBackgroundEventForNews: commandStubWithHandle(messagePointer, commandID, 0); break;
		case 0x0007: commandStub(messagePointer, commandID); break;
		case 0x0008: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0009: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x000A: commandStub(messagePointer, commandID); break;
		case 0x000B: commandStub(messagePointer, commandID); break;
		case 0x000C: commandStub(messagePointer, commandID); break;
		case 0x000D: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x000E: commandStub(messagePointer, commandID); break;
		case 0x000F: commandStub(messagePointer, commandID); break;
		case 0x0010: commandStub(messagePointer, commandID); break;
		case 0x0011: commandStub(messagePointer, commandID); break;
		case 0x0012: commandStub(messagePointer, commandID); break;
		case 0x0013: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0014: commandStub(messagePointer, commandID); break;
		case 0x0015: commandStubWithU32(messagePointer, commandID, 0); break;
		case NIMCommandID::GetNumAutoTitleDownloadTasks: commandStubWithU32(messagePointer, commandID, 0); break;
		case NIMCommandID::GetAutoTitleDownloadTaskInfos: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0018: commandStub(messagePointer, commandID); break;
		case 0x0019: commandStub(messagePointer, commandID); break;
		case 0x001A: commandStub(messagePointer, commandID); break;
		case 0x001B: commandStub(messagePointer, commandID); break;
		case 0x001C: commandStub(messagePointer, commandID); break;
		case 0x001D: commandStub(messagePointer, commandID); break;
		case 0x001E: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetTslXmlSize: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0020: commandStub(messagePointer, commandID); break;
		case NIMCommandID::Initialize: initialize(messagePointer); break;
		case 0x0022: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetDtlXmlSize: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0024: commandStub(messagePointer, commandID); break;
		case 0x0025: commandStub(messagePointer, commandID); break;
		case 0x0026: commandStub(messagePointer, commandID); break;
		case 0x0027: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetTitleDownloadProgress: getTitleDownloadProgress(messagePointer); break;
		case 0x0029: commandStub(messagePointer, commandID); break;
		case NIMCommandID::IsSystemUpdateAvailable: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x002B: commandStub(messagePointer, commandID); break;
		case 0x002C: commandStub(messagePointer, commandID); break;
		case 0x002D: commandStub(messagePointer, commandID); break;
		case 0x002E: commandStub(messagePointer, commandID); break;
		case NIMCommandID::StartDownload: commandStub(messagePointer, commandID); break;
		default: Helpers::panic("NIM service requested. Command: %08X\n", command);
	}
}

void NIMService::initialize(u32 messagePointer) {
	log("NIM::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::commandStub(u32 messagePointer, u32 commandID) {
	log("NIM::Command 0x%X (stubbed)\n", commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::commandStubWithU32(u32 messagePointer, u32 commandID, u32 value) {
	log("NIM::Command 0x%X (stubbed with u32)\n", commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, value);
}

void NIMService::commandStubWithHandle(u32 messagePointer, u32 commandID, Handle handle) {
	log("NIM::Command 0x%X (stubbed with handle)\n", commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Translation descriptor
	mem.write32(messagePointer + 12, handle);
}

void NIMService::getProgress(u32 messagePointer) {
	log("NIM::GetProgress (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2, 13, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// SystemUpdateProgress (0x28 bytes) + two u32 tail fields.
	for (u32 i = 0; i < 12; i++) {
		mem.write32(messagePointer + 8 + i * 4, 0);
	}
}

void NIMService::getTitleDownloadProgress(u32 messagePointer) {
	log("NIM::GetTitleDownloadProgress (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x28, 7, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// TitleDownloadProgress (0x18 bytes)
	for (u32 i = 0; i < 6; i++) {
		mem.write32(messagePointer + 8 + i * 4, 0);
	}
}
