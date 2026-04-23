#include "../../../include/services/pxi_dev.hpp"
#include "../../../include/ipc.hpp"

namespace PXICommands {
	enum : u32 {
		ReadHostIO = 0x00010042,
		WriteHostIO = 0x00020042,
		GetCardDevice = 0x000F0000,
	};
}

void PXIDevService::reset() {}

void PXIDevService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	const u32 commandID = command >> 16;

	switch (command) {
		case PXICommands::ReadHostIO: {
			log("PXI:DEV: ReadHostIO\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case PXICommands::WriteHostIO: {
			log("PXI:DEV: WriteHostIO\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case PXICommands::GetCardDevice: {
			log("PXI:DEV: GetCardDevice\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(0u); // 0 = NOR, 1 = NAND?
			break;
		}
		default:
			if (commandID >= 0x1 && commandID <= 0xF) {
				log("PXI:DEV command %04X (stubbed)\n", commandID);
				auto rb = rp.MakeBuilder(1, 0);
				rb.Push(Result::Success);
			} else {
				log("PXI:DEV unknown command: %08X\n", command);
				auto rb = rp.MakeBuilder(1, 0);
				rb.Push(Result::Success);
			}
			break;
	}
}
