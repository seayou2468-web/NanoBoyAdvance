#include "../../../include/services/qtm.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"

namespace QTMCommands {
	enum : u32 {
		InitializeHardwareCheck = 0x00010000,
		GetHeadtrackingInfoRaw = 0x00010040,
		GetHeadtrackingInfo = 0x00020080,
		SetIrLedCheck = 0x00050000,
	};
}

void QTMService::reset() {
	hardwareReady = false;
	irLedEnabled = false;
	sampleCounter = 0;
}

void QTMService::initializeHardwareCheck(u32 messagePointer) {
	log("QTM::InitializeHardwareCheck\n");
	hardwareReady = true;
	sampleCounter = 0;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void QTMService::setIrLedCheck(u32 messagePointer) {
	const u32 enabled = mem.read32(messagePointer + 4);
	irLedEnabled = enabled != 0;
	log("QTM::SetIrLedCheck (enabled=%u)\n", enabled);

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void QTMService::getHeadtrackingInfoRaw(u32 messagePointer, Type type) {
	const u32 outSize = mem.read32(messagePointer + 4);
	const u32 outPointer = mem.read32(messagePointer + 12);
	log("QTM::GetHeadtrackingInfoRaw (type=%d, size=%u, out=%08X)\n", static_cast<int>(type), outSize, outPointer);

	if (hardwareReady && irLedEnabled) {
		sampleCounter++;
	}

	const u32 yawRaw = sampleCounter * 10;
	const u32 pitchRaw = sampleCounter * 6;
	const u32 rollRaw = sampleCounter * 3;
	if (outSize >= 12) {
		mem.write32(outPointer, yawRaw);
		mem.write32(outPointer + 4, pitchRaw);
		mem.write32(outPointer + 8, rollRaw);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, std::min<u32>(outSize, 12));
}

void QTMService::getHeadtrackingInfo(u32 messagePointer, Type type) {
	log("QTM::GetHeadtrackingInfo (type=%d)\n", static_cast<int>(type));

	if (hardwareReady && irLedEnabled) {
		sampleCounter++;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 17, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// Synthetic headtracking sample block.
	const u32 yaw = sampleCounter * 10;
	const u32 pitch = sampleCounter * 6;
	const u32 roll = sampleCounter * 3;
	mem.write32(messagePointer + 8, hardwareReady ? 1 : 0);       // state
	mem.write32(messagePointer + 12, irLedEnabled ? 1 : 0);       // IR LED
	mem.write32(messagePointer + 16, sampleCounter);              // sequence
	mem.write32(messagePointer + 20, yaw);
	mem.write32(messagePointer + 24, pitch);
	mem.write32(messagePointer + 28, roll);
	for (u32 i = 24; i < 0x40; i += 4) {
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
