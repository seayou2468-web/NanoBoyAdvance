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

void NIMService::reset() {
	initialized = false;
	networkUpdateActive = false;
	titleDownloadActive = false;
	lastDownloadTitleIDLow = 0;
	lastDownloadTitleIDHigh = 0;
	systemUpdateProgress = 0;
	titleDownloadProgress = 0;
}

void NIMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16; // Lower bits encode IPC descriptors.

	switch (commandID) {
		case NIMCommandID::StartNetworkUpdate: startNetworkUpdate(messagePointer); break;
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
		case NIMCommandID::GetNumAutoTitleDownloadTasks: getNumAutoTitleDownloadTasks(messagePointer); break;
		case NIMCommandID::GetAutoTitleDownloadTaskInfos: getAutoTitleDownloadTaskInfos(messagePointer); break;
		case 0x0018: commandStub(messagePointer, commandID); break;
		case 0x0019: commandStub(messagePointer, commandID); break;
		case 0x001A: commandStub(messagePointer, commandID); break;
		case 0x001B: commandStub(messagePointer, commandID); break;
		case 0x001C: commandStub(messagePointer, commandID); break;
		case 0x001D: commandStub(messagePointer, commandID); break;
		case 0x001E: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetTslXmlSize: getTslXmlSize(messagePointer); break;
		case 0x0020: commandStub(messagePointer, commandID); break;
		case NIMCommandID::Initialize: initialize(messagePointer); break;
		case 0x0022: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetDtlXmlSize: getDtlXmlSize(messagePointer); break;
		case 0x0024: commandStub(messagePointer, commandID); break;
		case 0x0025: commandStub(messagePointer, commandID); break;
		case 0x0026: commandStub(messagePointer, commandID); break;
		case 0x0027: commandStub(messagePointer, commandID); break;
		case NIMCommandID::GetTitleDownloadProgress: getTitleDownloadProgress(messagePointer); break;
		case 0x0029: commandStub(messagePointer, commandID); break;
		case NIMCommandID::IsSystemUpdateAvailable: isSystemUpdateAvailable(messagePointer); break;
		case 0x002B: commandStub(messagePointer, commandID); break;
		case 0x002C: commandStub(messagePointer, commandID); break;
		case 0x002D: commandStub(messagePointer, commandID); break;
		case 0x002E: commandStub(messagePointer, commandID); break;
		case NIMCommandID::StartDownload: startDownload(messagePointer); break;
		default: Helpers::panic("NIM service requested. Command: %08X\n", command);
	}
}

void NIMService::initialize(u32 messagePointer) {
	log("NIM::Initialize\n");
	initialized = true;

	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::startNetworkUpdate(u32 messagePointer) {
	log("NIM::StartNetworkUpdate\n");

	networkUpdateActive = initialized;
	systemUpdateProgress = initialized ? 5 : 0;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::startDownload(u32 messagePointer) {
	lastDownloadTitleIDLow = mem.read32(messagePointer + 4);
	lastDownloadTitleIDHigh = mem.read32(messagePointer + 8);
	log("NIM::StartDownload (titleID=%08X%08X)\n", lastDownloadTitleIDHigh, lastDownloadTitleIDLow);

	titleDownloadActive = initialized;
	titleDownloadProgress = initialized ? 5 : 0;

	mem.write32(messagePointer, IPC::responseHeader(0x42, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::isSystemUpdateAvailable(u32 messagePointer) {
	log("NIM::IsSystemUpdateAvailable\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void NIMService::getNumAutoTitleDownloadTasks(u32 messagePointer) {
	log("NIM::GetNumAutoTitleDownloadTasks\n");

	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, titleDownloadActive ? 1 : 0);
}

void NIMService::getAutoTitleDownloadTaskInfos(u32 messagePointer) {
	const u32 maxTasks = mem.read32(messagePointer + 4);
	const u32 outPointer = mem.read32(messagePointer + 12);
	log("NIM::GetAutoTitleDownloadTaskInfos (max=%u, out=%08X)\n", maxTasks, outPointer);

	u32 writtenTasks = 0;
	if (titleDownloadActive && maxTasks != 0) {
		// Very small compatibility model: one active task represented by its title ID.
		mem.write32(outPointer, lastDownloadTitleIDLow);
		mem.write32(outPointer + 4, lastDownloadTitleIDHigh);
		mem.write32(outPointer + 8, titleDownloadProgress);
		mem.write32(outPointer + 12, 0);  // reserved
		writtenTasks = 1;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, writtenTasks);
}

void NIMService::getTslXmlSize(u32 messagePointer) {
	log("NIM::GetTslXmlSize\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void NIMService::getDtlXmlSize(u32 messagePointer) {
	log("NIM::GetDtlXmlSize\n");

	mem.write32(messagePointer, IPC::responseHeader(0x23, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
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
	log("NIM::GetProgress\n");
	if (networkUpdateActive && systemUpdateProgress < 100) {
		systemUpdateProgress += 10;
	}
	if (systemUpdateProgress >= 100) {
		systemUpdateProgress = 100;
		networkUpdateActive = false;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 13, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// SystemUpdateProgress (0x28 bytes) + two u32 tail fields.
	for (u32 i = 0; i < 12; i++) {
		mem.write32(messagePointer + 8 + i * 4, 0);
	}
	mem.write32(messagePointer + 8, systemUpdateProgress);
	mem.write32(messagePointer + 12, networkUpdateActive ? 1 : 0);
}

void NIMService::getTitleDownloadProgress(u32 messagePointer) {
	log("NIM::GetTitleDownloadProgress\n");
	if (titleDownloadActive && titleDownloadProgress < 100) {
		titleDownloadProgress += 10;
	}
	if (titleDownloadProgress >= 100) {
		titleDownloadProgress = 100;
		titleDownloadActive = false;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x28, 7, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// TitleDownloadProgress (0x18 bytes)
	for (u32 i = 0; i < 6; i++) {
		mem.write32(messagePointer + 8 + i * 4, 0);
	}
	mem.write32(messagePointer + 8, titleDownloadProgress);
	mem.write32(messagePointer + 12, titleDownloadActive ? 1 : 0);
	mem.write32(messagePointer + 16, lastDownloadTitleIDLow);
	mem.write32(messagePointer + 20, lastDownloadTitleIDHigh);
}
