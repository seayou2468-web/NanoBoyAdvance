#include "../../../include/services/ps_ps.hpp"

#include "../../../include/ipc.hpp"

namespace PSCommands {
	enum : u32 {
		SignRsaSha256 = 0x00010000,
		VerifyRsaSha256 = 0x00020000,
		EncryptDecryptAes = 0x00040000,
		EncryptSignDecryptVerifyAesCcm = 0x00050000,
		GetRomId = 0x00060000,
		GetRomId2 = 0x00070000,
		GetRomMakerCode = 0x00080000,
		GetCTRCardAutoStartupBit = 0x00090000,
		GetLocalFriendCodeSeed = 0x000A0000,
		GetDeviceId = 0x000B0000,
		SeedRNG = 0x000C0000,
		GenerateRandomBytes = 0x000D0000,
		InterfaceForPXI_0x04010084 = 0x000E0000,
		InterfaceForPXI_0x04020082 = 0x000F0000,
		InterfaceForPXI_0x04030044 = 0x00100000,
		InterfaceForPXI_0x04040044 = 0x00110000,
	};
}

void PSPSService::reset() {
	std::random_device rd;
	rng.seed(rd());

	// Keep PS identity responses aligned with other Aurora3DS services (CFG/FRD),
	// which currently expose zeroed identifiers.
	deviceID = 0;
	localFriendCodeSeed = 0;
}

void PSPSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command & 0xFFFF0000) {
		case PSCommands::GenerateRandomBytes: generateRandomBytes(messagePointer); break;
		case PSCommands::SeedRNG: seedRNG(messagePointer); break;
		case PSCommands::SignRsaSha256: stubCommand(messagePointer, "SignRsaSha256"); break;
		case PSCommands::VerifyRsaSha256: stubCommand(messagePointer, "VerifyRsaSha256"); break;
		case PSCommands::EncryptDecryptAes: stubCommand(messagePointer, "EncryptDecryptAes"); break;
		case PSCommands::EncryptSignDecryptVerifyAesCcm: stubCommand(messagePointer, "EncryptSignDecryptVerifyAesCcm"); break;
		case PSCommands::GetRomId: getRomID(messagePointer, false); break;
		case PSCommands::GetRomId2: getRomID(messagePointer, true); break;
		case PSCommands::GetRomMakerCode: getRomMakerCode(messagePointer); break;
		case PSCommands::GetCTRCardAutoStartupBit: getCTRCardAutoStartupBit(messagePointer); break;
		case PSCommands::GetLocalFriendCodeSeed: getLocalFriendCodeSeed(messagePointer); break;
		case PSCommands::GetDeviceId: getDeviceID(messagePointer); break;
		case PSCommands::InterfaceForPXI_0x04010084: stubCommand(messagePointer, "InterfaceForPXI_0x04010084"); break;
		case PSCommands::InterfaceForPXI_0x04020082: stubCommand(messagePointer, "InterfaceForPXI_0x04020082"); break;
		case PSCommands::InterfaceForPXI_0x04030044: stubCommand(messagePointer, "InterfaceForPXI_0x04030044"); break;
		case PSCommands::InterfaceForPXI_0x04040044: stubCommand(messagePointer, "InterfaceForPXI_0x04040044"); break;
		default: Helpers::panic("ps:ps service requested. Command: %08X\n", command);
	}
}

void PSPSService::seedRNG(u32 messagePointer) {
	const u32 seed = mem.read32(messagePointer + 4);
	log("ps:ps::SeedRNG (seed=%08X)\n", seed);
	rng.seed(seed);

	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PSPSService::generateRandomBytes(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 descriptor = mem.read32(messagePointer + 8);
	const u32 buffer = mem.read32(messagePointer + 12);

	log("ps:ps::GenerateRandomBytes (size=%u, buffer=%08X)\n", size, buffer);

	for (u32 i = 0; i < size; i++) {
		mem.write8(buffer + i, u8(rng() & 0xFF));
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, descriptor);
	mem.write32(messagePointer + 12, buffer);
}

void PSPSService::getRomID(u32 messagePointer, bool secondary) {
	log("ps:ps::GetRomId%s\n", secondary ? "2" : "");
	mem.write32(messagePointer, IPC::responseHeader(secondary ? 0x7 : 0x6, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, 0);  // No cartridge data in HLE mode
}

void PSPSService::getRomMakerCode(u32 messagePointer) {
	log("ps:ps::GetRomMakerCode\n");
	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, 0);
}

void PSPSService::getCTRCardAutoStartupBit(u32 messagePointer) {
	log("ps:ps::GetCTRCardAutoStartupBit\n");
	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);
}

void PSPSService::getLocalFriendCodeSeed(u32 messagePointer) {
	log("ps:ps::GetLocalFriendCodeSeed\n");
	mem.write32(messagePointer, IPC::responseHeader(0xA, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, localFriendCodeSeed);
}

void PSPSService::getDeviceID(u32 messagePointer) {
	log("ps:ps::GetDeviceId\n");
	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, deviceID);
}

void PSPSService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("ps:ps::%s (command=%08X) [stubbed]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
