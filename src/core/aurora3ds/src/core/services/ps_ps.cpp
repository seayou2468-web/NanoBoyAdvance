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
	};
}

void PSPSService::reset() {
	std::random_device rd;
	rng.seed(rd());
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
		case PSCommands::GetRomId: stubCommand(messagePointer, "GetRomId"); break;
		case PSCommands::GetRomId2: stubCommand(messagePointer, "GetRomId2"); break;
		case PSCommands::GetRomMakerCode: stubCommand(messagePointer, "GetRomMakerCode"); break;
		case PSCommands::GetCTRCardAutoStartupBit: stubCommand(messagePointer, "GetCTRCardAutoStartupBit"); break;
		case PSCommands::GetLocalFriendCodeSeed: stubCommand(messagePointer, "GetLocalFriendCodeSeed"); break;
		case PSCommands::GetDeviceId: stubCommand(messagePointer, "GetDeviceId"); break;
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

void PSPSService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("ps:ps::%s (command=%08X) [stubbed]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
