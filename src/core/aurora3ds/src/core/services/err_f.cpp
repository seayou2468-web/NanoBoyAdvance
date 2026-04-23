#include "../../../include/services/err_f.hpp"

#include "../../../include/ipc.hpp"

namespace ErrFCommands {
	enum : u32 {
		ThrowFatalError = 0x00010800
	};
}

void ErrFService::reset() {}

void ErrFService::throwFatalError(u32 messagePointer) {
	log("ERR:F::ThrowFatalError (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ErrFService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case ErrFCommands::ThrowFatalError: throwFatalError(messagePointer); break;
		default: Helpers::panic("ERR:F service requested. Command: %08X\n", command);
	}
}
