#include "../../../include/services/nwm_ext.hpp"

#include "../../../include/ipc.hpp"

namespace NWMExtCommands {
	enum : u32 {
		GetWirelessEnabled = 0x00010000,
		SetCommunicationMode = 0x00020040,
		GetCommunicationMode = 0x00030000,
		SetPolicy = 0x00040040,
		GetPolicy = 0x00050000,
		ControlWirelessEnabled = 0x00080040,
	};
}

void NwmExtService::reset() {
	wirelessEnabled = true;
	communicationMode = 0;
	lastPolicy = 0;
}

void NwmExtService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command & 0xFFFF0000) {
		case (NWMExtCommands::GetWirelessEnabled & 0xFFFF0000): getWirelessEnabled(messagePointer); break;
		case (NWMExtCommands::SetCommunicationMode & 0xFFFF0000): setCommunicationMode(messagePointer); break;
		case (NWMExtCommands::GetCommunicationMode & 0xFFFF0000): getCommunicationMode(messagePointer); break;
		case (NWMExtCommands::SetPolicy & 0xFFFF0000): setPolicy(messagePointer); break;
		case (NWMExtCommands::GetPolicy & 0xFFFF0000): getPolicy(messagePointer); break;
		case (NWMExtCommands::ControlWirelessEnabled & 0xFFFF0000): controlWirelessEnabled(messagePointer); break;
		default: stubCommand(messagePointer, "Unknown");
	}
}

void NwmExtService::getWirelessEnabled(u32 messagePointer) {
	log("nwm::EXT::GetWirelessEnabled\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, wirelessEnabled ? 1u : 0u);
}

void NwmExtService::controlWirelessEnabled(u32 messagePointer) {
	const u32 enabled = mem.read32(messagePointer + 4);
	log("nwm::EXT::ControlWirelessEnabled (%u)\n", enabled);
	wirelessEnabled = enabled != 0;

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::setCommunicationMode(u32 messagePointer) {
	communicationMode = mem.read32(messagePointer + 4);
	log("nwm::EXT::SetCommunicationMode (%u)\n", communicationMode);
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::getCommunicationMode(u32 messagePointer) {
	log("nwm::EXT::GetCommunicationMode\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, communicationMode);
}

void NwmExtService::setPolicy(u32 messagePointer) {
	lastPolicy = mem.read32(messagePointer + 4);
	log("nwm::EXT::SetPolicy (%u)\n", lastPolicy);
	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::getPolicy(u32 messagePointer) {
	log("nwm::EXT::GetPolicy\n");
	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, lastPolicy);
}

void NwmExtService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("nwm::EXT::%s (command=%08X) [fallback]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, wirelessEnabled ? 1u : 0u);
}
