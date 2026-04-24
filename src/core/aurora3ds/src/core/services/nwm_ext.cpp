#include "../../../include/services/nwm_ext.hpp"

#include <algorithm>
#include <array>

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

namespace NWMSocCommands {
	enum : u32 {
		GetMACAddress = 0x00080040,
	};
}

namespace NWMInfCommands {
	enum : u32 {
		RecvBeaconBroadcastData = 0x00060000,
		ConnectToEncryptedAP = 0x00070000,
		ConnectToAP = 0x00080000,
	};
}

namespace NWMCecCommands {
	enum : u32 {
		SendProbeRequest = 0x000D0000,
	};
}

void NwmExtService::reset() {
	wirelessEnabled = true;
	communicationMode = 0;
	lastPolicy = 0;
}

void NwmExtService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);

	switch (type) {
		case Type::EXT:
			switch (command & 0xFFFF0000) {
				case (NWMExtCommands::GetWirelessEnabled & 0xFFFF0000): getWirelessEnabled(messagePointer); break;
				case (NWMExtCommands::SetCommunicationMode & 0xFFFF0000): setCommunicationMode(messagePointer); break;
				case (NWMExtCommands::GetCommunicationMode & 0xFFFF0000): getCommunicationMode(messagePointer); break;
				case (NWMExtCommands::SetPolicy & 0xFFFF0000): setPolicy(messagePointer); break;
				case (NWMExtCommands::GetPolicy & 0xFFFF0000): getPolicy(messagePointer); break;
				case (NWMExtCommands::ControlWirelessEnabled & 0xFFFF0000): controlWirelessEnabled(messagePointer); break;
				default: stubCommand(messagePointer, "Unknown");
			}
			break;

		case Type::SOC:
			switch (command & 0xFFFF0000) {
				case (NWMSocCommands::GetMACAddress & 0xFFFF0000): getMacAddress(messagePointer); break;
				default: stubCommand(messagePointer, "SOC::Unknown");
			}
			break;

		case Type::INF:
			switch (command & 0xFFFF0000) {
				case (NWMInfCommands::RecvBeaconBroadcastData & 0xFFFF0000):
					stubCommand(messagePointer, "INF::RecvBeaconBroadcastData");
					break;
				case (NWMInfCommands::ConnectToEncryptedAP & 0xFFFF0000):
					stubCommand(messagePointer, "INF::ConnectToEncryptedAP");
					break;
				case (NWMInfCommands::ConnectToAP & 0xFFFF0000): stubCommand(messagePointer, "INF::ConnectToAP"); break;
				default: genericSuccess(messagePointer);
			}
			break;

		case Type::CEC:
			if ((command & 0xFFFF0000) == (NWMCecCommands::SendProbeRequest & 0xFFFF0000)) {
				stubCommand(messagePointer, "CEC::SendProbeRequest");
			} else {
				genericSuccess(messagePointer);
			}
			break;

		case Type::SAP:
		case Type::TST: genericSuccess(messagePointer); break;
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

void NwmExtService::writePseudoMac(u32 outPointer, u32 outSize) {
	static constexpr std::array<u8, 6> DefaultMac = {0x02, 0x00, 0x3D, 0x0A, 0x55, 0x01};
	const u32 copySize = std::min<u32>(outSize, static_cast<u32>(DefaultMac.size()));
	for (u32 i = 0; i < copySize; i++) {
		mem.write8(outPointer + i, DefaultMac[i]);
	}
}

void NwmExtService::getMacAddress(u32 messagePointer) {
	const u32 outSize = mem.read32(messagePointer + 4);
	const u32 outPointer = mem.read32(messagePointer + 12);
	log("nwm::SOC::GetMACAddress (size=%u, pointer=%08X)\n", outSize, outPointer);

	writePseudoMac(outPointer, outSize);
	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::genericSuccess(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	log("nwm service generic success fallback (command=%08X)\n", command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NwmExtService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("nwm::EXT::%s (command=%08X) [fallback]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, wirelessEnabled ? 1u : 0u);
}
