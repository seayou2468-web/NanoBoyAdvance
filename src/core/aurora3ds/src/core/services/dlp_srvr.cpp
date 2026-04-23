#include "../../../include/services/dlp_srvr.hpp"

#include "../../../include/ipc.hpp"

namespace DlpSrvrCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		Finalize = 0x00020000,
		GetServerState = 0x00030000,
		GetEventDescription = 0x00040000,
		StartHosting = 0x00050082,
		EndHosting = 0x00060000,
		StartDistribution = 0x00070000,
		BeginGame = 0x00080000,
		AcceptClient = 0x00090042,
		DisconnectClient = 0x000A0040,
		GetConnectingClients = 0x000B0000,
		GetClientInfo = 0x000C0040,
		GetClientState = 0x000D0042,
		IsChild = 0x000E0040,
		InitializeWithName = 0x000F0042,
		GetDupNoticeNeed = 0x00100000,
	};
}

namespace DlpClntCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		Finalize = 0x00020000,
		GetEventDescription = 0x00030000,
		GetChannels = 0x00040000,
		StartScan = 0x00050082,
		StopScan = 0x00060000,
		GetServerInfo = 0x00070042,
		GetTitleInfo = 0x00080042,
		GetTitleInfoInOrder = 0x00090040,
		DeleteScanInfo = 0x000A0042,
		PrepareForSystemDownload = 0x000B0042,
		StartSystemDownload = 0x000C0000,
		StartSession = 0x000D0042,
		GetMyStatus = 0x000E0000,
		GetConnectingNodes = 0x000F0000,
		GetNodeInfo = 0x00100040,
		GetWirelessRebootPassphrase = 0x00110000,
		StopSession = 0x00120000,
		GetCupVersion = 0x00130042,
		GetDupAvailability = 0x00140042,
	};
}

namespace DlpFkclCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		Finalize = 0x00020000,
		GetEventDescription = 0x00030000,
		GetChannels = 0x00040000,
		StartScan = 0x00050082,
		StopScan = 0x00060000,
		GetServerInfo = 0x00070042,
		GetTitleInfo = 0x00080042,
		GetTitleInfoInOrder = 0x00090040,
		DeleteScanInfo = 0x000A0042,
		StartSession = 0x000B0042,
		GetMyStatus = 0x000C0000,
		GetConnectingNodes = 0x000D0000,
		GetNodeInfo = 0x000E0040,
		GetWirelessRebootPassphrase = 0x000F0000,
		StopSession = 0x00100000,
		InitializeWithName = 0x00110042,
	};
}

void DlpSrvrService::reset() {
	initialized = false;
	scanning = false;
	hosting = false;
	distributing = false;
	gameStarted = false;
	connectedClients = 0;
}

void DlpSrvrService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	switch (type) {
		case Type::SRVR:
			switch (command) {
				case DlpSrvrCommands::Initialize: initialize(messagePointer, false); break;
				case DlpSrvrCommands::Finalize: finalize(messagePointer); break;
				case DlpSrvrCommands::GetServerState: getServerState(messagePointer); break;
				case DlpSrvrCommands::GetEventDescription: getEmptyBufferResponse(messagePointer, 0x4); break;
				case DlpSrvrCommands::StartHosting: startHosting(messagePointer); break;
				case DlpSrvrCommands::EndHosting: endHosting(messagePointer); break;
				case DlpSrvrCommands::StartDistribution: startDistribution(messagePointer); break;
				case DlpSrvrCommands::BeginGame: beginGame(messagePointer); break;
				case DlpSrvrCommands::AcceptClient: acceptClient(messagePointer); break;
				case DlpSrvrCommands::DisconnectClient: disconnectClient(messagePointer); break;
				case DlpSrvrCommands::GetConnectingClients: getConnectingClients(messagePointer); break;
				case DlpSrvrCommands::GetClientInfo: getClientInfo(messagePointer); break;
				case DlpSrvrCommands::GetClientState: getClientState(messagePointer); break;
				case DlpSrvrCommands::IsChild: isChild(messagePointer); break;
				case DlpSrvrCommands::InitializeWithName: initialize(messagePointer, true); break;
				case DlpSrvrCommands::GetDupNoticeNeed: getDupNoticeNeed(messagePointer); break;
				default: Helpers::panic("DLP::SRVR service requested. Command: %08X\n", command);
			}
			break;
		case Type::CLNT: {
			switch (command) {
				case DlpClntCommands::Initialize: initialize(messagePointer, false); break;
				case DlpClntCommands::Finalize: finalize(messagePointer); break;
				case DlpClntCommands::GetEventDescription: getEmptyBufferResponse(messagePointer, 0x3); break;
				case DlpClntCommands::GetChannels: getChannels(messagePointer); break;
				case DlpClntCommands::StartScan: startScan(messagePointer); break;
				case DlpClntCommands::StopScan: stopScan(messagePointer); break;
				case DlpClntCommands::GetServerInfo: getEmptyBufferResponse(messagePointer, 0x7); break;
				case DlpClntCommands::GetTitleInfo: getEmptyBufferResponse(messagePointer, 0x8); break;
				case DlpClntCommands::GetTitleInfoInOrder: getEmptyBufferResponse(messagePointer, 0x9); break;
				case DlpClntCommands::DeleteScanInfo: getEmptyBufferResponse(messagePointer, 0xA); break;
				case DlpClntCommands::PrepareForSystemDownload: prepareForSystemDownload(messagePointer); break;
				case DlpClntCommands::StartSystemDownload: startSystemDownload(messagePointer); break;
				case DlpClntCommands::StartSession: getEmptyBufferResponse(messagePointer, 0xD); break;
				case DlpClntCommands::GetMyStatus: getMyStatus(messagePointer); break;
				case DlpClntCommands::GetConnectingNodes: getEmptyBufferResponse(messagePointer, 0xF); break;
				case DlpClntCommands::GetNodeInfo: getEmptyBufferResponse(messagePointer, 0x10); break;
				case DlpClntCommands::GetWirelessRebootPassphrase: getEmptyBufferResponse(messagePointer, 0x11); break;
				case DlpClntCommands::StopSession: getEmptyBufferResponse(messagePointer, 0x12); break;
				case DlpClntCommands::GetCupVersion: getCupVersion(messagePointer); break;
				case DlpClntCommands::GetDupAvailability: getDupAvailability(messagePointer); break;
				default:
					Helpers::panic("DLP::CLNT service requested. Command: %08X\n", command);
			}
			break;
		}
		case Type::FKCL: {
			switch (command) {
				case DlpFkclCommands::Initialize: initialize(messagePointer, false); break;
				case DlpFkclCommands::InitializeWithName: initialize(messagePointer, true); break;
				case DlpFkclCommands::Finalize: finalize(messagePointer); break;
				case DlpFkclCommands::GetEventDescription: getEmptyBufferResponse(messagePointer, 0x3); break;
				case DlpFkclCommands::GetChannels: getChannels(messagePointer); break;
				case DlpFkclCommands::StartScan: startScan(messagePointer); break;
				case DlpFkclCommands::StopScan: stopScan(messagePointer); break;
				case DlpFkclCommands::GetServerInfo: getEmptyBufferResponse(messagePointer, 0x7); break;
				case DlpFkclCommands::GetTitleInfo: getEmptyBufferResponse(messagePointer, 0x8); break;
				case DlpFkclCommands::GetTitleInfoInOrder: getEmptyBufferResponse(messagePointer, 0x9); break;
				case DlpFkclCommands::DeleteScanInfo: getEmptyBufferResponse(messagePointer, 0xA); break;
				case DlpFkclCommands::StartSession: getEmptyBufferResponse(messagePointer, 0xB); break;
				case DlpFkclCommands::GetMyStatus: getMyStatus(messagePointer); break;
				case DlpFkclCommands::GetConnectingNodes: getEmptyBufferResponse(messagePointer, 0xD); break;
				case DlpFkclCommands::GetNodeInfo: getEmptyBufferResponse(messagePointer, 0xE); break;
				case DlpFkclCommands::GetWirelessRebootPassphrase: getEmptyBufferResponse(messagePointer, 0xF); break;
				case DlpFkclCommands::StopSession: getEmptyBufferResponse(messagePointer, 0x10); break;
				default:
					Helpers::panic("DLP::FKCL service requested. Command: %08X\n", command);
			}
			break;
		}
	}
}

void DlpSrvrService::isChild(u32 messagePointer) {
	log("DLP::SRVR: IsChild\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // We are responsible adults
}

void DlpSrvrService::getServerState(u32 messagePointer) {
	const u32 state = initialized ? (hosting ? 2u : 1u) : 0u;
	mem.write32(messagePointer, IPC::responseHeader(0x3, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state);
}

void DlpSrvrService::startHosting(u32 messagePointer) {
	hosting = initialized;
	distributing = false;
	gameStarted = false;
	connectedClients = 0;
	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, hosting ? Result::Success : Result::OS::InvalidCombination);
}

void DlpSrvrService::endHosting(u32 messagePointer) {
	hosting = false;
	distributing = false;
	gameStarted = false;
	connectedClients = 0;
	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::startDistribution(u32 messagePointer) {
	distributing = hosting;
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, distributing ? Result::Success : Result::OS::InvalidCombination);
}

void DlpSrvrService::beginGame(u32 messagePointer) {
	gameStarted = distributing;
	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, gameStarted ? Result::Success : Result::OS::InvalidCombination);
}

void DlpSrvrService::acceptClient(u32 messagePointer) {
	if (connectedClients < 15) {
		connectedClients++;
	}
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::disconnectClient(u32 messagePointer) {
	if (connectedClients > 0) {
		connectedClients--;
	}
	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::getConnectingClients(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, connectedClients);
}

void DlpSrvrService::getClientInfo(u32 messagePointer) {
	getEmptyBufferResponse(messagePointer, 0xC);
}

void DlpSrvrService::getClientState(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, gameStarted ? 2u : (distributing ? 1u : 0u));
}

void DlpSrvrService::getDupNoticeNeed(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x10, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void DlpSrvrService::initialize(u32 messagePointer, bool withName) {
	log("DLP::CLNT/FKCL: Initialize%s\n", withName ? "WithName" : "");
	initialized = true;
	scanning = false;
	hosting = false;
	distributing = false;
	gameStarted = false;
	connectedClients = 0;

	mem.write32(messagePointer, IPC::responseHeader(withName ? 0x11 : 0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::finalize(u32 messagePointer) {
	log("DLP::CLNT/FKCL: Finalize\n");
	initialized = false;
	scanning = false;
	hosting = false;
	distributing = false;
	gameStarted = false;
	connectedClients = 0;

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::getChannels(u32 messagePointer) {
	log("DLP::CLNT/FKCL: GetChannels\n");

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, channelHandle);
}

void DlpSrvrService::startScan(u32 messagePointer) {
	log("DLP::CLNT/FKCL: StartScan\n");
	scanning = initialized;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, initialized ? Result::Success : Result::OS::InvalidCombination);
}

void DlpSrvrService::stopScan(u32 messagePointer) {
	log("DLP::CLNT/FKCL: StopScan\n");
	scanning = false;

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::getEmptyBufferResponse(u32 messagePointer, u32 commandID) {
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, 0);
}

void DlpSrvrService::getMyStatus(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xE, 6, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, initialized ? (scanning ? 0x02000000 : 0x01000000) : 0);
	mem.write32(messagePointer + 12, 0);
	mem.write32(messagePointer + 16, 0);
	mem.write32(messagePointer + 20, 0);
	mem.write32(messagePointer + 24, 0);
}

void DlpSrvrService::prepareForSystemDownload(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void DlpSrvrService::startSystemDownload(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DlpSrvrService::getDupAvailability(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x14, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void DlpSrvrService::getCupVersion(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0x13, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, 0);
}
