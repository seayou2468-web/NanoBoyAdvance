#include "../../../include/services/nfc.hpp"

#include "../../../include/io_file.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include <algorithm>

namespace NFCCommands {
	enum : u32 {
		Initialize = 0x00010040,
		Shutdown = 0x00020040,
		StartCommunication = 0x00030000,
		StopCommunication = 0x00040000,
		StartTagScanning = 0x00050040,
		StopTagScanning = 0x00060000,
		Mount = 0x00070000,
		Unmount = 0x00080000,
		Flush = 0x00090002,
		GetTagInRangeEvent = 0x000B0000,
		GetTagOutOfRangeEvent = 0x000C0000,
		GetTagState = 0x000D0000,
		CommunicationGetStatus = 0x000F0000,
		GetTagInfo2 = 0x00100000,
		GetTagInfo = 0x00110000,
		CommunicationGetResult = 0x00120000,
		OpenApplicationArea = 0x00130040,
		CreateApplicationArea = 0x00140384,
		ReadApplicationArea = 0x00150040,
		WriteApplicationArea = 0x00160242,
		GetNfpRegisterInfo = 0x00170000,
		GetNfpCommonInfo = 0x00180000,
		InitializeCreateInfo = 0x00190000,
		LoadAmiiboPartially = 0x001A0000,
		GetModelInfo = 0x001B0000, // Also used by newer stacks as GetIdentificationBlock
		Format = 0x040100C2,
		GetAdminInfo = 0x04020000,
		GetEmptyRegisterInfo = 0x04030000,
		SetRegisterInfo = 0x04040042,
		DeleteRegisterInfo = 0x04050000,
		DeleteApplicationArea = 0x04060000,
		ExistsApplicationArea = 0x04070000,
	};
}

void NFCService::reset() {
	device.reset();
	tagInRangeEvent = std::nullopt;
	tagOutOfRangeEvent = std::nullopt;

	adapterStatus = Old3DSAdapterStatus::Idle;
	tagStatus = TagStatus::NotInitialized;
	initialized = false;
	appAreaCreated = false;
	appAreaOpen = false;
	appAreaID = 0;
	appAreaSize = 0;
	appAreaData.fill(0);
}

void NFCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NFCCommands::CommunicationGetResult: communicationGetResult(messagePointer); break;
		case NFCCommands::CommunicationGetStatus: communicationGetStatus(messagePointer); break;
		case NFCCommands::Initialize: initialize(messagePointer); break;
		case NFCCommands::GetModelInfo: getModelInfo(messagePointer); break;
		case NFCCommands::GetTagInfo2: getIdentificationBlock(messagePointer); break;
		case NFCCommands::GetTagInfo: getTagInfo(messagePointer); break;
		case NFCCommands::GetTagInRangeEvent: getTagInRangeEvent(messagePointer); break;
		case NFCCommands::GetTagOutOfRangeEvent: getTagOutOfRangeEvent(messagePointer); break;
		case NFCCommands::GetTagState: getTagState(messagePointer); break;
		case NFCCommands::Mount: mount(messagePointer); break;
		case NFCCommands::Unmount: unmount(messagePointer); break;
		case NFCCommands::Flush: flush(messagePointer); break;
		case NFCCommands::OpenApplicationArea: openApplicationArea(messagePointer); break;
		case NFCCommands::CreateApplicationArea: createApplicationArea(messagePointer); break;
		case NFCCommands::ReadApplicationArea: readApplicationArea(messagePointer); break;
		case NFCCommands::WriteApplicationArea: writeApplicationArea(messagePointer); break;
		case NFCCommands::GetNfpRegisterInfo: getNfpRegisterInfo(messagePointer); break;
		case NFCCommands::GetNfpCommonInfo: getNfpCommonInfo(messagePointer); break;
		case NFCCommands::InitializeCreateInfo: initializeCreateInfo(messagePointer); break;
		case NFCCommands::LoadAmiiboPartially: loadAmiiboPartially(messagePointer); break;
		case NFCCommands::Format: format(messagePointer); break;
		case NFCCommands::GetAdminInfo: getAdminInfo(messagePointer); break;
		case NFCCommands::GetEmptyRegisterInfo: getEmptyRegisterInfo(messagePointer); break;
		case NFCCommands::SetRegisterInfo: setRegisterInfo(messagePointer); break;
		case NFCCommands::DeleteRegisterInfo: deleteRegisterInfo(messagePointer); break;
		case NFCCommands::DeleteApplicationArea: deleteApplicationArea(messagePointer); break;
		case NFCCommands::ExistsApplicationArea: existsApplicationArea(messagePointer); break;
		case NFCCommands::Shutdown: shutdown(messagePointer); break;
		case NFCCommands::StartCommunication: startCommunication(messagePointer); break;
		case NFCCommands::StartTagScanning: startTagScanning(messagePointer); break;
		case NFCCommands::StopCommunication: stopCommunication(messagePointer); break;
		case NFCCommands::StopTagScanning: stopTagScanning(messagePointer); break;
		default: Helpers::panic("NFC service requested. Command: %08X\n", command);
	}
}

bool NFCService::loadAmiibo(const std::filesystem::path& path) {
	if (!initialized || tagStatus != TagStatus::Scanning) {
		Helpers::warn("It's not the correct time to load an amiibo! Make sure to load amiibi when the game is searching for one!");
		return false;
	}

	IOFile file(path, "rb");
	if (!file.isOpen()) {
		printf("Failed to open Amiibo file");
		file.close();

		return false;
	}

	auto [success, bytesRead] = file.readBytes(&device.raw, AmiiboDevice::tagSize);
	if (!success || bytesRead != AmiiboDevice::tagSize) {
		printf("Failed to read entire tag from Amiibo file: File might not be a proper amiibo file\n");
		file.close();

		return false;
	}

	device.loadFromRaw();

	if (tagOutOfRangeEvent.has_value()) {
		kernel.clearEvent(tagOutOfRangeEvent.value());
	}

	if (tagInRangeEvent.has_value()) {
		kernel.signalEvent(tagInRangeEvent.value());
	}

	file.close();
	return true;
}

void NFCService::initialize(u32 messagePointer) {
	const u8 type = mem.read8(messagePointer + 4);
	log("NFC::Initialize (type = %d)\n", type);

	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	tagStatus = TagStatus::Initialized;
	initialized = true;
	// TODO: This should error if already initialized. Also sanitize type.
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::shutdown(u32 messagePointer) {
	log("MFC::Shutdown");
	const u8 mode = mem.read8(messagePointer + 4);

	Helpers::warn("NFC::Shutdown: Unimplemented mode: %d", mode);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

/*
	The NFC service provides userland with 2 events. One that is signaled when an NFC tag gets in range,
	And one that is signaled when it gets out of range. Userland can have a thread sleep on this so it will be alerted
	Whenever an Amiibo or misc NFC tag is presented or removed.
	These events are retrieved via the GetTagInRangeEvent and GetTagOutOfRangeEvent function respectively
*/

void NFCService::getTagInRangeEvent(u32 messagePointer) {
	log("NFC::GetTagInRangeEvent\n");

	// Create event if it doesn't exist
	if (!tagInRangeEvent.has_value()) {
		tagInRangeEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0B, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here
	mem.write32(messagePointer + 12, tagInRangeEvent.value());
}

void NFCService::getTagOutOfRangeEvent(u32 messagePointer) {
	log("NFC::GetTagOutOfRangeEvent\n");

	// Create event if it doesn't exist
	if (!tagOutOfRangeEvent.has_value()) {
		tagOutOfRangeEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here
	mem.write32(messagePointer + 12, tagOutOfRangeEvent.value());
}

void NFCService::getTagState(u32 messagePointer) {
	log("NFC::GetTagState\n");

	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u8>(tagStatus));
}

void NFCService::communicationGetStatus(u32 messagePointer) {
	log("NFC::CommunicationGetStatus\n");

	if (!initialized) {
		Helpers::warn("NFC::CommunicationGetStatus: Old 3DS NFC Adapter not initialized\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u32>(adapterStatus));
}

void NFCService::communicationGetResult(u32 messagePointer) {
	log("NFC::CommunicationGetResult\n");

	if (!initialized) {
		Helpers::warn("NFC::CommunicationGetResult: Old 3DS NFC Adapter not initialized\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0x12, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// On N3DS: This always writes 0 here
	// On O3DS with the NFC adapter: Returns a result code for NFC communication
	mem.write32(messagePointer + 8, 0);
}

void NFCService::startCommunication(u32 messagePointer) {
	log("NFC::StartCommunication\n");
	// adapterStatus = Old3DSAdapterStatus::Active;
	// TODO: Actually start communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::startTagScanning(u32 messagePointer) {
	log("NFC::StartTagScanning\n");
	if (!initialized) {
		Helpers::warn("Scanning for NFC tags before NFC service is initialized");
	}

	tagStatus = TagStatus::Scanning;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopTagScanning(u32 messagePointer) {
	log("NFC::StopTagScanning\n");
	if (!initialized) {
		Helpers::warn("Stopping scanning for NFC tags before NFC service is initialized");
	}

	tagStatus = TagStatus::Initialized;

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopCommunication(u32 messagePointer) {
	log("NFC::StopCommunication\n");
	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	// TODO: Actually stop communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getTagInfo(u32 messagePointer) {
	log("NFC::GetTagInfo\n");

	mem.write32(messagePointer, IPC::responseHeader(0x11, 12, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, device.isLoaded() ? 1 : 0);  // in-range flag
	mem.write32(messagePointer + 12, 2);                          // TagType::Type2
	mem.write32(messagePointer + 16, 1);                          // TagProtocol::TypeA

	const auto& uid = device.getUID();
	for (u32 i = 0; i < uid.size(); i++) {
		mem.write8(messagePointer + 20 + i, uid[i]);
	}
	mem.write8(messagePointer + 27, 0);  // Nintendo manufacturer ID high byte (placeholder)
	mem.write16(messagePointer + 28, 0); // lock bytes placeholder
}

void NFCService::loadAmiiboPartially(u32 messagePointer) {
	log("NFC::LoadAmiiboPartially\n");
	if (!device.isLoaded()) {
		Helpers::warn("NFC::LoadAmiiboPartially called without a loaded amiibo");
	}
	tagStatus = device.isLoaded() ? TagStatus::Loaded : tagStatus;

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::mount(u32 messagePointer) {
	log("NFC::Mount\n");
	if (!initialized || !device.isLoaded()) {
		mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	tagStatus = TagStatus::Loaded;
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::unmount(u32 messagePointer) {
	log("NFC::Unmount\n");
	appAreaOpen = false;
	tagStatus = device.isLoaded() ? TagStatus::InRange : TagStatus::Initialized;

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::flush(u32 messagePointer) {
	log("NFC::Flush\n");
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::openApplicationArea(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	log("NFC::OpenApplicationArea (appID=%08X)\n", appID);

	if (!appAreaCreated || appAreaID != appID) {
		mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	appAreaOpen = true;
	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::createApplicationArea(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	const u16 size = static_cast<u16>(mem.read32(messagePointer + 8));
	const u32 dataPointer = mem.read32(messagePointer + 48);
	log("NFC::CreateApplicationArea (appID=%08X, size=%u, data=%08X)\n", appID, size, dataPointer);

	appAreaID = appID;
	appAreaSize = std::min<u16>(size, static_cast<u16>(appAreaData.size()));
	appAreaCreated = true;
	appAreaOpen = true;
	appAreaData.fill(0);

	for (u16 i = 0; i < appAreaSize; i++) {
		appAreaData[i] = mem.read8(dataPointer + i);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::readApplicationArea(u32 messagePointer) {
	const u16 size = static_cast<u16>(mem.read32(messagePointer + 4));
	const u32 outPointer = mem.read32(messagePointer + 12);
	log("NFC::ReadApplicationArea (size=%u, out=%08X)\n", size, outPointer);

	if (!appAreaCreated || !appAreaOpen) {
		mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	const u16 copySize = std::min<u16>(size, appAreaSize);
	for (u16 i = 0; i < copySize; i++) {
		mem.write8(outPointer + i, appAreaData[i]);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::writeApplicationArea(u32 messagePointer) {
	const u16 size = static_cast<u16>(mem.read32(messagePointer + 4));
	const u32 inPointer = mem.read32(messagePointer + 40);
	log("NFC::WriteApplicationArea (size=%u, in=%08X)\n", size, inPointer);

	if (!appAreaCreated || !appAreaOpen) {
		mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		return;
	}

	appAreaSize = std::min<u16>(size, static_cast<u16>(appAreaData.size()));
	for (u16 i = 0; i < appAreaSize; i++) {
		appAreaData[i] = mem.read8(inPointer + i);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getNfpRegisterInfo(u32 messagePointer) {
	log("NFC::GetNfpRegisterInfo\n");
	mem.write32(messagePointer, IPC::responseHeader(0x17, 11, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, device.getCharacterID());
	mem.write16(messagePointer + 10, static_cast<u16>(device.getModelNumber()));
	mem.write8(messagePointer + 12, device.getSeries());
	mem.write8(messagePointer + 13, appAreaCreated ? 1 : 0);
	mem.write32(messagePointer + 16, appAreaID);
	mem.write16(messagePointer + 20, appAreaSize);
}

void NFCService::getNfpCommonInfo(u32 messagePointer) {
	log("NFC::GetNfpCommonInfo\n");
	mem.write32(messagePointer, IPC::responseHeader(0x18, 6, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, device.isLoaded() ? 1 : 0);
	mem.write32(messagePointer + 12, appAreaCreated ? 1 : 0);
	mem.write32(messagePointer + 16, static_cast<u32>(tagStatus));
	mem.write32(messagePointer + 20, appAreaSize);
}

void NFCService::initializeCreateInfo(u32 messagePointer) {
	log("NFC::InitializeCreateInfo\n");
	mem.write32(messagePointer, IPC::responseHeader(0x19, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getIdentificationBlock(u32 messagePointer) {
	log("NFC::GetIdentificationBlock\n");
	const auto& uid = device.getUID();
	mem.write32(messagePointer, IPC::responseHeader(0x10, 15, 0));
	mem.write32(messagePointer + 4, Result::Success);
	for (u32 i = 0; i < uid.size(); i++) {
		mem.write8(messagePointer + 8 + i, uid[i]);
	}
}

void NFCService::format(u32 messagePointer) {
	log("NFC::Format\n");
	appAreaCreated = false;
	appAreaOpen = false;
	appAreaID = 0;
	appAreaSize = 0;
	appAreaData.fill(0);

	mem.write32(messagePointer, IPC::responseHeader(0x401, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getAdminInfo(u32 messagePointer) {
	log("NFC::GetAdminInfo\n");
	mem.write32(messagePointer, IPC::responseHeader(0x402, 6, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, appAreaCreated ? 1 : 0);
	mem.write32(messagePointer + 12, appAreaOpen ? 1 : 0);
	mem.write32(messagePointer + 16, appAreaID);
	mem.write32(messagePointer + 20, appAreaSize);
}

void NFCService::getEmptyRegisterInfo(u32 messagePointer) {
	log("NFC::GetEmptyRegisterInfo\n");
	mem.write32(messagePointer, IPC::responseHeader(0x403, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, appAreaCreated ? 0 : 1);
}

void NFCService::setRegisterInfo(u32 messagePointer) {
	const u32 newAppID = mem.read32(messagePointer + 8);
	log("NFC::SetRegisterInfo (appID=%08X)\n", newAppID);

	appAreaID = newAppID;
	mem.write32(messagePointer, IPC::responseHeader(0x404, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::deleteRegisterInfo(u32 messagePointer) {
	log("NFC::DeleteRegisterInfo\n");
	appAreaID = 0;
	appAreaOpen = false;
	mem.write32(messagePointer, IPC::responseHeader(0x405, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::deleteApplicationArea(u32 messagePointer) {
	log("NFC::DeleteApplicationArea\n");
	appAreaCreated = false;
	appAreaOpen = false;
	appAreaID = 0;
	appAreaSize = 0;
	appAreaData.fill(0);

	mem.write32(messagePointer, IPC::responseHeader(0x406, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::existsApplicationArea(u32 messagePointer) {
	log("NFC::ExistsApplicationArea\n");
	mem.write32(messagePointer, IPC::responseHeader(0x407, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, appAreaCreated ? 1 : 0);
}

void NFCService::getModelInfo(u32 messagePointer) {
	log("NFC::GetModelInfo\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1B, 14, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, device.getCharacterID());
	mem.write8(messagePointer + 10, 0);                      // character variant
	mem.write8(messagePointer + 11, 0);                      // AmiiboType::Figure
	mem.write16(messagePointer + 12, device.getModelNumber());
	mem.write8(messagePointer + 14, device.getSeries());     // AmiiboSeries
	mem.write8(messagePointer + 15, 2);                      // PackedTagType::Type2
	mem.write32(messagePointer + 16, 0);                     // reserved
	mem.write32(messagePointer + 20, device.isLoaded() ? 1 : 0);
}
