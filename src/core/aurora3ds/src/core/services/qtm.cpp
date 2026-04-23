#include "../../../include/services/qtm.hpp"
#include "../../../include/ipc.hpp"

namespace QTMCommands {
	enum : u32 {
		InitializeHardwareCheck = 0x00010000,
		GetHeadtrackingInfo = 0x00020042,
	};
}

void QTMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case QTMCommands::InitializeHardwareCheck: {
			log("QTM: InitializeHardwareCheck\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case QTMCommands::GetHeadtrackingInfo: {
			log("QTM: GetHeadtrackingInfo\n");
			auto rb = rp.MakeBuilder(1, 2);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("QTM unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}
