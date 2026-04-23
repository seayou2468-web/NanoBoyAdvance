#include "../../../include/services/plgldr.hpp"
#include "../../../include/ipc.hpp"

namespace PLGLDRCommands {
	enum : u32 {
		LoadPlugin = 0x00010000,
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
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case PLGLDRCommands::IsEnabled: {
			log("plg:ldr::IsEnabled\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(enabled ? 1u : 0u);
			break;
		}
		case PLGLDRCommands::SetEnabled: {
			enabled = rp.Pop() != 0;
			log("plg:ldr::SetEnabled (%d)\n", enabled);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case PLGLDRCommands::GetPLGLDRVersion: {
			log("plg:ldr::GetPLGLDRVersion\n");
			auto rb = rp.MakeBuilder(4, 0);
			rb.Push(Result::Success);
			rb.Push(1u);
			rb.Push(0u);
			rb.Push(2u);
			break;
		}
		default: {
			log("plg:ldr unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
	}
}
