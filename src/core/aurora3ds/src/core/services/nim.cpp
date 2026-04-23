#include "../../../include/services/nim.hpp"

#include "../../../include/ipc.hpp"

namespace NIMCommands {
	enum : u32 {
		// nim:u
		StartNetworkUpdate = 0x00010000,
		GetProgress = 0x00020000,
		Cancel = 0x00030000,
		CommitSystemTitles = 0x00040000,
		GetBackgroundEventForMenu = 0x00050000,
		GetBackgroundEventForNews = 0x00060000,
		FormatSaveData = 0x00070000,
		GetCustomerSupportCode = 0x00080000,
		IsCommittableAllSystemTitles = 0x00090000,
		GetBackgroundProgress = 0x000A0000,
		GetSavedHash = 0x000B0000,
		UnregisterTask = 0x000C0000,
		IsRegistered = 0x000D0000,
		FindTaskInfo = 0x000E0000,
		GetTaskInfos = 0x000F0000,
		DeleteUnmanagedContexts = 0x00100000,
		UpdateAutoTitleDownloadTasksAsync = 0x00110000,
		StartPendingAutoTitleDownloadTasksAsync = 0x00120000,
		GetAsyncResult = 0x00130000,
		CancelAsyncCall = 0x00140000,
		IsPendingAutoTitleDownloadTasks = 0x00150000,
		GetNumAutoTitleDownloadTasks = 0x00160000,
		GetAutoTitleDownloadTaskInfos = 0x00170000,
		CancelAutoTitleDownloadTask = 0x00180000,
		SetAutoDbgDat = 0x00190000,
		GetAutoDbgDat = 0x001A0000,
		SetDbgTasks = 0x001B0000,
		GetDbgTasks = 0x001C0000,
		DeleteDbgData = 0x001D0000,
		SetTslXml = 0x001E0000,
		GetTslXmlSize = 0x001F0000,
		GetTslXml = 0x00200000,
		Initialize = 0x00210000,
		DeleteTslXml = 0x00210000,
		SetDtlXml = 0x00220000,
		GetDtlXmlSize = 0x00230000,
		GetDtlXml = 0x00240000,
		UpdateAccountStatus = 0x00250000,
		StartTitleDownload = 0x00260000,
		StopTitleDownload = 0x00270000,
		GetTitleDownloadProgress = 0x00280000,
		RegisterTask = 0x00290000,
		IsSystemUpdateAvailable = 0x002A0000,
		Unknown2B = 0x002B0000,
		UpdateTickets = 0x002C0000,
		DownloadTitleSeedAsync = 0x002D0000,
		DownloadMissingTitleSeedsAsync = 0x002E0000,

		// nim:s
		CheckSysupdateAvailableSOAP = 0x000A0000,
		ListTitles = 0x00160000,
		AccountCheckBalanceSOAP = 0x00290000,
		DownloadTickets = 0x002D0000,
		StartDownload = 0x00420000,

		// nim:aoc
		SetApplicationId = 0x00030000,
		SetTin = 0x00040000,
		ListContentSetsEx = 0x00090000,
		GetBalance = 0x00180000,
		GetCustomerSupportCodeAOC = 0x001D0000,
		InitializeAOC = 0x00210000,
		CalculateContentsRequiredSize = 0x00240000,
		RefreshServerTime = 0x00250000,
	};
}

void NIMService::reset() {}

void NIMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16; // Lower bits encode IPC descriptors.

	switch (commandID) {
		case 0x0001: commandStub(messagePointer, commandID); break;
		case 0x0002: getProgress(messagePointer); break;
		case 0x0003: commandStub(messagePointer, commandID); break;
		case 0x0004: commandStub(messagePointer, commandID); break;
		case 0x0005: commandStubWithHandle(messagePointer, commandID, 0); break;
		case 0x0006: commandStubWithHandle(messagePointer, commandID, 0); break;
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
		case 0x0016: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0017: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0018: commandStub(messagePointer, commandID); break;
		case 0x0019: commandStub(messagePointer, commandID); break;
		case 0x001A: commandStub(messagePointer, commandID); break;
		case 0x001B: commandStub(messagePointer, commandID); break;
		case 0x001C: commandStub(messagePointer, commandID); break;
		case 0x001D: commandStub(messagePointer, commandID); break;
		case 0x001E: commandStub(messagePointer, commandID); break;
		case 0x001F: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0020: commandStub(messagePointer, commandID); break;
		case 0x0021: initialize(messagePointer); break;
		case 0x0022: commandStub(messagePointer, commandID); break;
		case 0x0023: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x0024: commandStub(messagePointer, commandID); break;
		case 0x0025: commandStub(messagePointer, commandID); break;
		case 0x0026: commandStub(messagePointer, commandID); break;
		case 0x0027: commandStub(messagePointer, commandID); break;
		case 0x0028: getTitleDownloadProgress(messagePointer); break;
		case 0x0029: commandStub(messagePointer, commandID); break;
		case 0x002A: commandStubWithU32(messagePointer, commandID, 0); break;
		case 0x002B: commandStub(messagePointer, commandID); break;
		case 0x002C: commandStub(messagePointer, commandID); break;
		case 0x002D: commandStub(messagePointer, commandID); break;
		case 0x002E: commandStub(messagePointer, commandID); break;
		case 0x0042: commandStub(messagePointer, commandID); break;
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
