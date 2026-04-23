#include "../../../include/services/news_u.hpp"
#include "../../../include/ipc.hpp"

namespace NewsCommands {
	enum : u32 {
		AddNotification = 0x00010002,
	};
}

void NewsUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NewsCommands::AddNotification: addNotification(messagePointer); break;
		default:
			log("NEWS:U unknown command: %08X\n", command);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void NewsUService::addNotification(u32 messagePointer) {
	log("NEWS:U::AddNotification\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
