#include "../../../include/services/qtm.hpp"

#include "../../../include/ipc.hpp"

namespace QTMCommands {
	enum : u32 {
		InitializeHardwareCheck = 0x00010000,
		GetHeadtrackingInfoRaw = 0x00010040,
		GetHeadtrackingInfo = 0x00020080,
		SetIrLedCheck = 0x00050000,
	};
}

void QTMService::reset() {}

void QTMService::initializeHardwareCheck(u32 messagePointer) {
	log("QTM::InitializeHardwareCheck (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void QTMService::setIrLedCheck(u32 messagePointer) {
	log("QTM::SetIrLedCheck (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void QTMService::getHeadtrackingInfoRaw(u32 messagePointer, Type type) {
	log("QTM::GetHeadtrackingInfoRaw (type=%d, stubbed)\n", static_cast<int>(type));

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void QTMService::getHeadtrackingInfo(u32 messagePointer, Type type) {
	log("QTM::GetHeadtrackingInfo (type=%d, stubbed)\n", static_cast<int>(type));

	mem.write32(messagePointer, IPC::responseHeader(0x2, 17, 0));
	mem.write32(messagePointer + 4, Result::Success);

	for (u32 i = 0; i < 0x40; i += 4) {
		mem.write32(messagePointer + 8 + i, 0);
	}
}

void QTMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);

	switch (type) {
		case Type::C:
			switch (command) {
				case QTMCommands::InitializeHardwareCheck: initializeHardwareCheck(messagePointer); break;
				case QTMCommands::SetIrLedCheck: setIrLedCheck(messagePointer); break;
				default: Helpers::panic("qtm:c service requested. Command: %08X\n", command);
			}
			break;
		case Type::S:
		case Type::SP:
		case Type::U:
			switch (command) {
				case QTMCommands::GetHeadtrackingInfoRaw: getHeadtrackingInfoRaw(messagePointer, type); break;
				case QTMCommands::GetHeadtrackingInfo: getHeadtrackingInfo(messagePointer, type); break;
				default: Helpers::panic("qtm:* service requested. Command: %08X\n", command);
			}
			break;
	}
}
