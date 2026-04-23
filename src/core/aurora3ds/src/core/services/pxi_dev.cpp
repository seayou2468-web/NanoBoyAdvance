#include "../../../include/services/pxi_dev.hpp"

#include "../../../include/ipc.hpp"

void PXIDevService::reset() {}

void PXIDevService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	// Reference implementation only registers stub handlers for commands 0x1-0xF.
	if (commandID >= 0x1 && commandID <= 0xF) {
		log("PXI:DEV command %04X (stubbed)\n", commandID);
		mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
		mem.write32(messagePointer + 4, Result::Success);
		return;
	}

	Helpers::panic("PXI:DEV service requested. Command: %08X\n", command);
}
