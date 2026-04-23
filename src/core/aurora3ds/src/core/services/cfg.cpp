#include "../../../include/services/cfg.hpp"
#include <array>
#include <string>
#include "../../../include/ipc.hpp"
#include "../../../include/system_models.hpp"

namespace CFGCommands {
	enum : u32 {
		GetConfig = 0x00010082,
		GetRegion = 0x00020000,
		GetSystemModel = 0x00050000,
		TranslateCountryInfo = 0x00080080,
		GetCountryCodeString = 0x00090040,
		GetCountryCodeID = 0x000A0040,
		IsFangateSupported = 0x000B0000,
		GetSystemConfig = 0x04010082,
		SetSystemConfig = 0x04020082,
		UpdateConfigNANDSavegame = 0x04030000,
		GetLocalFriendCodeSeed = 0x04050000,
		SecureInfoGetByte101 = 0x04070000,
	};
}

void CFGService::reset() {}

void CFGService::handleSyncRequest(u32 messagePointer, CFGService::Type type) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case CFGCommands::GetSystemModel: {
			log("CFG::GetSystemModel\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(static_cast<u32>(config.isNew3DS ? 2 : 0)); // 0=O3DS, 2=N3DS
			break;
		}
		case CFGCommands::GetRegion: {
			log("CFG::GetRegion\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(static_cast<u32>(config.region));
			break;
		}
		case CFGCommands::SecureInfoGetByte101: {
			log("CFG::SecureInfoGetByte101\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(0u);
			break;
		}
		default:
			log("CFG service requested unknown command: %08X for type %d\n", command, static_cast<int>(type));
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void CFGService::getSystemModel(u32 messagePointer) {
	// Re-implemented in handleSyncRequest
}

void CFGService::secureInfoGetRegion(u32 messagePointer) {
	// Re-implemented in handleSyncRequest
}

// Implement other stubs
void CFGService::getConfigInfoBlk2(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::getConfigInfoBlk8(u32 messagePointer, u32 command) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::getCountryCodeID(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::getCountryCodeString(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::getLocalFriendCodeSeed(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::getRegionCanadaUSA(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::genUniqueConsoleHash(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::isFangateSupported(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::secureInfoGetByte101(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::setConfigInfoBlk4(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::translateCountryInfo(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::updateConfigNANDSavegame(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::norInitialize(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void CFGService::norReadData(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
