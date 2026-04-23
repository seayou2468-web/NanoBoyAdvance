#include "../../../include/services/ldr_ro.hpp"
#include "../../../include/ipc.hpp"

namespace LDRCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		LoadCRR = 0x00020082,
		UnloadCRR = 0x00030040,
		LoadCRO = 0x000402C2,
		UnloadCRO = 0x000500C2,
		LinkCRO = 0x00060042,
		UnlinkCRO = 0x00070042,
	};
}

void LDRService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case LDRCommands::Initialize: {
			log("LDR:RO: Initialize\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case LDRCommands::LoadCRR: {
			log("LDR:RO: LoadCRR\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("LDR:RO unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}
