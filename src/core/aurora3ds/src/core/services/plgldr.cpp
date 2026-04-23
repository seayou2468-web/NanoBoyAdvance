#include "../../../include/services/plgldr.hpp"

#include "../../../include/ipc.hpp"

namespace PLGLDRCommands {
	enum : u32 {
		IsEnabled = 0x00020000,
		SetEnabled = 0x00030040,
		GetPLGLDRVersion = 0x00080000,
	};
}

void PLGLDRService::reset() {
	enabled = false;
}

void PLGLDRService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case PLGLDRCommands::IsEnabled:
			log("plg:ldr::IsEnabled\n");
			mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
			mem.write32(messagePointer + 4, Result::Success);
			mem.write32(messagePointer + 8, enabled ? 1 : 0);
			break;
		case PLGLDRCommands::SetEnabled:
			enabled = mem.read32(messagePointer + 4) != 0;
			log("plg:ldr::SetEnabled (%d)\n", enabled ? 1 : 0);
			mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
		case PLGLDRCommands::GetPLGLDRVersion:
			log("plg:ldr::GetPLGLDRVersion\n");
			mem.write32(messagePointer, IPC::responseHeader(0x8, 4, 0));
			mem.write32(messagePointer + 4, Result::Success);
			mem.write32(messagePointer + 8, 1);
			mem.write32(messagePointer + 12, 0);
			mem.write32(messagePointer + 16, 2);
			break;
		default: {
			const u32 commandID = command >> 16;
			log("plg:ldr command %08X (stubbed)\n", command);
			mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
		}
	}
}
