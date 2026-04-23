#include "../../../include/services/ns.hpp"

#include "../../../include/ipc.hpp"

namespace NSCommands {
	enum : u32 {
		LaunchTitle = 0x000200C0,
		LockSpecialContent = 0x00010000,
		UnlockSpecialContent = 0x00020000,
	};
}

void NSService::reset() {}

void NSService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);

	switch (type) {
		case Type::S:
			switch (command) {
				case NSCommands::LaunchTitle: launchTitle(messagePointer); break;
				default: Helpers::panic("NS:s service requested. Command: %08X\n", command);
			}
			break;
		case Type::C:
			switch (command) {
				case NSCommands::LockSpecialContent:
				case NSCommands::UnlockSpecialContent:
					log("NS:c command %08X [stubbed]\n", command);
					mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
					mem.write32(messagePointer + 4, Result::Success);
					break;
				default: Helpers::panic("NS:c service requested. Command: %08X\n", command);
			}
			break;
		default:
			Helpers::panic("NS service requested. Command: %08X\n", command);
			break;
	}
}

void NSService::launchTitle(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	const u32 launchFlags = mem.read32(messagePointer + 12);
	Helpers::warn("NS::LaunchTitle (title ID = %llX, launch flags = %X) (stubbed)", titleID, launchFlags);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Process ID
}
