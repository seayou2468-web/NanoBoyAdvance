#include "../../../include/services/act.hpp"

#include "../../../include/ipc.hpp"

namespace ACTCommands {
	enum : u32 {
		Initialize = 0x00010084,
		GetErrorCode = 0x00020040,
		GetAccountDataBlock = 0x000600C2,
		GenerateUUID = 0x000D0040,
	};
}

void ACTService::reset() {
	nextUuidCounter = 1;
	lastResultCode = Result::Success;
}

void ACTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ACTCommands::GetErrorCode: getErrorCode(messagePointer); break;
		case ACTCommands::GenerateUUID: generateUUID(messagePointer); break;
		case ACTCommands::GetAccountDataBlock: getAccountDataBlock(messagePointer); break;
		case ACTCommands::Initialize: initialize(messagePointer); break;
		default: respondNotImplemented(messagePointer, command); break;
	}
}

void ACTService::initialize(u32 messagePointer) {
	const u32 sdkVersion = mem.read32(messagePointer + 4);
	const u32 sharedMemorySize = mem.read32(messagePointer + 8);
	log("ACT::Initialize (sdkVersion=%08X, sharedMemorySize=%08X)\n", sdkVersion, sharedMemorySize);
	lastResultCode = Result::Success;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ACTService::getErrorCode(u32 messagePointer) {
	const u32 resultCode = mem.read32(messagePointer + 4);
	log("ACT::GetErrorCode (resultCode=%08X)\n", resultCode);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, resultCode);
	lastResultCode = resultCode;
}

void ACTService::generateUUID(u32 messagePointer) {
	const u32 input = mem.read32(messagePointer + 4);
	const u32 outputPointer = mem.read32(messagePointer + 20);
	log("ACT::GenerateUUID (input=%08X, output=%08X)\n", input, outputPointer);

	// Deterministic pseudo UUID blob (16 bytes), enough for software expecting non-zero unique data.
	const u32 seed = 0xA8C30000u | (nextUuidCounter++ & 0xFFFF);
	if (outputPointer != 0) {
		mem.write32(outputPointer + 0, seed);
		mem.write32(outputPointer + 4, seed ^ 0x13579BDFu);
		mem.write32(outputPointer + 8, seed ^ 0x2468ACE0u);
		mem.write32(outputPointer + 12, seed ^ 0xFEEDC0DEu);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	lastResultCode = Result::Success;
}

void ACTService::getAccountDataBlock(u32 messagePointer) {
	const u8 accountSlot = mem.read8(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 blkID = mem.read32(messagePointer + 12);
	const u32 outputPointer = mem.read32(messagePointer + 20);
	log("ACT::GetAccountDataBlock (slot=%u, size=%08X, block=%08X, output=%08X)\n", accountSlot, size, blkID, outputPointer);

	// Minimal deterministic payload to satisfy callers that require initialized account block data.
	for (u32 i = 0; i < size; i++) {
		const u8 value = static_cast<u8>((blkID + accountSlot + i) & 0xFF);
		mem.write8(outputPointer + i, value);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	lastResultCode = Result::Success;
}

void ACTService::respondNotImplemented(u32 messagePointer, u32 command) {
	const u32 commandID = command >> 16;
	Helpers::warn("Undocumented ACT service requested. Command: %08X", command);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
	lastResultCode = Result::OS::NotImplemented;
}
