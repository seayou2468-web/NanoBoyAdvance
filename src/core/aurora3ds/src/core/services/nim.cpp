#include "../../../include/services/nim.hpp"

#include "../../../include/ipc.hpp"

namespace NIMCommands {
	enum : u32 {
		Initialize = 0x00210000,
	};
}

void NIMService::reset() {}

void NIMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	switch (commandID) {
		// nim:u
		case 0x1:
		case 0x3:
		case 0x4:
		case 0x7:
		case 0x8:
		case 0x9:
		case 0xA:
		case 0xB:
		case 0xC:
		case 0xD:
		case 0xE:
		case 0xF:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0x20:
		case 0x22:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
			commandStub(messagePointer, commandID);
			break;
		case 0x2: commandStubWithU32(messagePointer, commandID, 0); break;   // GetProgress
		case 0x16: commandStubWithU32(messagePointer, commandID, 0); break;  // GetNumAutoTitleDownloadTasks
		case 0x17: commandStubWithU32(messagePointer, commandID, 0); break;  // GetAutoTitleDownloadTaskInfos
		case 0x1F: commandStubWithU32(messagePointer, commandID, 0); break;  // GetTslXmlSize
		case 0x23: commandStubWithU32(messagePointer, commandID, 0); break;  // GetDtlXmlSize
		case 0x28: commandStubWithU32(messagePointer, commandID, 0); break;  // GetTitleDownloadProgress

		// nim:s
		case 0x42: commandStub(messagePointer, commandID); break;

		// nim:aoc
		case 0x5:
		case 0x6:
			commandStub(messagePointer, commandID);
			break;
		default:
			if (command == NIMCommands::Initialize) {
				initialize(messagePointer);
			} else if (commandID == 0x3 || commandID == 0x4 || commandID == 0x9 || commandID == 0x18 ||
					   commandID == 0x1D || commandID == 0x24 || commandID == 0x25) {
				// nim:aoc command IDs overlap with nim:u IDs; generic stub is sufficient.
				commandStub(messagePointer, commandID);
			} else {
				Helpers::panic("NIM service requested. Command: %08X\n", command);
			}
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
