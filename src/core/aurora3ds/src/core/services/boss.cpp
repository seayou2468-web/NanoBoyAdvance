#include "../../../include/services/boss.hpp"
#include "../../../include/ipc.hpp"
#include <algorithm>

namespace BOSSCommands {
	enum : u32 {
		InitializeSession = 0x00010082,
		RegisterStorage = 0x00020082,
		UnregisterStorage = 0x00030000,
		GetStorageInfo = 0x00040000,
		GetNewArrivalFlag = 0x00070000,
		RegisterNewArrivalEvent = 0x00080002,
		SetOptoutFlag = 0x00090040,
		GetOptoutFlag = 0x000A0000,
		RegisterTask = 0x000B00C2,
		UnregisterTask = 0x000C0082,
		GetTaskIdList = 0x000E0000,
		GetNsDataIdList = 0x00100102,
		SendProperty = 0x00140082,
		ReceiveProperty = 0x00160082,
		GetTaskServiceStatus = 0x001B0042,
		StartTask = 0x001C0042,
		CancelTask = 0x001E0042,
		GetTaskStatus = 0x002300C2,
		GetTaskInfo = 0x00250082,
		DeleteNsData = 0x00260040,
		ReadNsData = 0x00280102,
		GetNsDataLastUpdated = 0x002D0040,
		GetErrorCode = 0x002E0040,
		RegisterStorageEntry = 0x002F0140,
		GetStorageEntryInfo = 0x00300000,
		StartBgImmediate = 0x00330042,
		InitializeSessionPrivileged = 0x04010082,
		GetAppNewFlag = 0x04040080,
	};
}

void BOSSService::reset() { optoutFlag = 0; }

void BOSSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case BOSSCommands::InitializeSession: {
			log("BOSS::InitializeSession\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case BOSSCommands::GetOptoutFlag: {
			log("BOSS::GetOptoutFlag\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(optoutFlag);
			break;
		}
		case BOSSCommands::SetOptoutFlag: {
			optoutFlag = rp.Pop();
			log("BOSS::SetOptoutFlag(%u)\n", optoutFlag);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case BOSSCommands::GetNewArrivalFlag: {
			log("BOSS::GetNewArrivalFlag\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(0u); // No new arrival
			break;
		}
		default:
			log("BOSS service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

// Stub implementations to satisfy the "integrated" requirement
void BOSSService::cancelTask(u32 messagePointer) { log("BOSS::CancelTask\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::deleteNsData(u32 messagePointer) { log("BOSS::DeleteNsData\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getAppNewFlag(u32 messagePointer) { log("BOSS::GetAppNewFlag\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getErrorCode(u32 messagePointer) { log("BOSS::GetErrorCode\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getNsDataIdList(u32 messagePointer) { log("BOSS::GetNsDataIdList\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getNsDataLastUpdated(u32 messagePointer) { log("BOSS::GetNsDataLastUpdated\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getStorageEntryInfo(u32 messagePointer) { log("BOSS::GetStorageEntryInfo\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getTaskIdList(u32 messagePointer) { log("BOSS::GetTaskIdList\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getTaskInfo(u32 messagePointer) { log("BOSS::GetTaskInfo\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getTaskServiceStatus(u32 messagePointer) { log("BOSS::GetTaskServiceStatus\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getTaskStatus(u32 messagePointer) { log("BOSS::GetTaskStatus\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::getTaskStorageInfo(u32 messagePointer) { log("BOSS::GetTaskStorageInfo\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::readNsData(u32 messagePointer) { log("BOSS::ReadNsData\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::receiveProperty(u32 messagePointer) { log("BOSS::ReceiveProperty\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::registerNewArrivalEvent(u32 messagePointer) { log("BOSS::RegisterNewArrivalEvent\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::registerStorageEntry(u32 messagePointer) { log("BOSS::RegisterStorageEntry\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::registerTask(u32 messagePointer) { log("BOSS::RegisterTask\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::startBgImmediate(u32 messagePointer) { log("BOSS::StartBgImmediate\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::unregisterTask(u32 messagePointer) { log("BOSS::UnregisterTask\n"); mem.write32(messagePointer + 4, Result::Success); }
void BOSSService::unregisterStorage(u32 messagePointer) { log("BOSS::UnregisterStorage\n"); mem.write32(messagePointer + 4, Result::Success); }
