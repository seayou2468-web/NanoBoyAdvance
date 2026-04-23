#include "../../../include/services/err_f.hpp"
#include "../../../include/ipc.hpp"

namespace ErrCommands {
	enum : u32 {
		ThrowFatalError = 0x00010040,
	};
}

void ErrFService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ErrCommands::ThrowFatalError: throwFatalError(messagePointer); break;
		default:
			log("ERR:F unknown command: %08X\n", command);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void ErrFService::throwFatalError(u32 messagePointer) {
	log("ERR:F::ThrowFatalError\n");
	IPC::RequestParser rp(messagePointer, mem);
	// In reference, it just logs and returns success (or doesn't return if it's fatal)
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
