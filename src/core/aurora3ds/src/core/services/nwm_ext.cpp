#include "../../../include/services/nwm_ext.hpp"

#include "../../../include/ipc.hpp"

namespace NWMExtCommands {
	enum : u32 {
		ControlWirelessEnabled = 0x00080040,
	};
}

void NwmExtService::reset() {}

void NwmExtService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command & 0xFFFF0000) {
		case (NWMExtCommands::ControlWirelessEnabled & 0xFFFF0000): controlWirelessEnabled(messagePointer); break;
		default: stubCommand(messagePointer, "Unknown");
	}
}

void NwmExtService::controlWirelessEnabled(u32 messagePointer) {
	const u32 enabled = mem.read32(messagePointer + 4);
	log("nwm::EXT::ControlWirelessEnabled (%u) [stubbed]\n", enabled);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("nwm::EXT::%s (command=%08X) [stubbed]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
