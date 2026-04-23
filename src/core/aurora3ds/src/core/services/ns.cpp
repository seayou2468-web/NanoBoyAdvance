#include "../../../include/services/ns.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"

namespace NSCommands {
	enum : u32 {
		LockSpecialContent = 0x00010000,
		UnlockSpecialContent = 0x00020000,

		LaunchFIRM = 0x00010000,
		LaunchTitle = 0x000200C0,
		TerminateApplication = 0x00030040,
		TerminateProcess = 0x00040040,
		LaunchApplicationFIRM = 0x00050040,
		SetWirelessRebootInfo = 0x00060082,
		CardUpdateInitialize = 0x00070000,
		CardUpdateShutdown = 0x00080000,
		SetTWLBannerHMAC = 0x000D0000,
		ShutdownAsync = 0x000E0000,
		RebootSystem = 0x00100040,
		TerminateTitle = 0x00110040,
		SetApplicationCpuTimeLimit = 0x00120080,
		LaunchApplication = 0x00150040,
		RebootSystemClean = 0x00160000,
	};
}

void NSService::reset() {
	wirelessRebootInfo.fill(0);
	pendingShutdown = false;
}

void NSService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);

	switch (type) {
		case Type::S:
			switch (command) {
				case NSCommands::LaunchFIRM: launchFIRM(messagePointer); break;
				case NSCommands::LaunchTitle: launchTitle(messagePointer); break;
				case NSCommands::TerminateApplication: terminateApplication(messagePointer); break;
				case NSCommands::TerminateProcess: terminateProcess(messagePointer); break;
				case NSCommands::LaunchApplicationFIRM: launchApplicationFIRM(messagePointer); break;
				case NSCommands::SetWirelessRebootInfo: setWirelessRebootInfo(messagePointer); break;
				case NSCommands::CardUpdateInitialize: cardUpdateInitialize(messagePointer); break;
				case NSCommands::CardUpdateShutdown: cardUpdateShutdown(messagePointer); break;
				case NSCommands::SetTWLBannerHMAC:
					log("NS::SetTWLBannerHMAC\n");
					mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
					mem.write32(messagePointer + 4, Result::Success);
					break;
				case NSCommands::ShutdownAsync: shutdownAsync(messagePointer); break;
				case NSCommands::RebootSystem: rebootSystem(messagePointer); break;
				case NSCommands::TerminateTitle: terminateTitle(messagePointer); break;
				case NSCommands::SetApplicationCpuTimeLimit: setApplicationCpuTimeLimit(messagePointer); break;
				case NSCommands::LaunchApplication: launchApplication(messagePointer); break;
				case NSCommands::RebootSystemClean: rebootSystemClean(messagePointer); break;
				default: Helpers::panic("NS:s service requested. Command: %08X\n", command);
			}
			break;
		case Type::C:
		case Type::P:
			switch (command) {
				case NSCommands::LockSpecialContent:
				case NSCommands::UnlockSpecialContent:
					log("NS:c command %08X [success]\n", command);
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

void NSService::launchFIRM(u32 messagePointer) {
	const u32 launchFlags = mem.read32(messagePointer + 4);
	log("NS::LaunchFIRM (launch flags = %X)\n", launchFlags);

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::launchTitle(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	const u32 launchFlags = mem.read32(messagePointer + 12);
	log("NS::LaunchTitle (title ID = %llX, launch flags = %X)\n", titleID, launchFlags);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Process ID (not currently surfaced by Aurora runtime)
}

void NSService::terminateApplication(u32 messagePointer) {
	const u64 timeout = mem.read64(messagePointer + 4);
	log("NS::TerminateApplication (timeout = %llu)\n", timeout);

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::terminateProcess(u32 messagePointer) {
	const u64 timeout = mem.read64(messagePointer + 4);
	log("NS::TerminateProcess (timeout = %llu)\n", timeout);

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::launchApplicationFIRM(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("NS::LaunchApplicationFIRM (title ID = %llX)\n", titleID);

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::setWirelessRebootInfo(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 8);
	const u32 pointer = mem.read32(messagePointer + 16);
	const u32 copySize = std::min<u32>(size, WirelessRebootInfoBufferSize);
	for (u32 i = 0; i < copySize; i++) {
		wirelessRebootInfo[i] = mem.read8(pointer + i);
	}
	for (u32 i = copySize; i < WirelessRebootInfoBufferSize; i++) {
		wirelessRebootInfo[i] = 0;
	}
	log("NS::SetWirelessRebootInfo (size = %u, stored = %u)\n", size, copySize);

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::cardUpdateInitialize(u32 messagePointer) {
	log("NS::CardUpdateInitialize\n");

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::cardUpdateShutdown(u32 messagePointer) {
	log("NS::CardUpdateShutdown\n");

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::shutdownAsync(u32 messagePointer) {
	pendingShutdown = true;
	log("NS::ShutdownAsync\n");

	mem.write32(messagePointer, IPC::responseHeader(0xE, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::rebootSystem(u32 messagePointer) {
	const u64 timeout = mem.read64(messagePointer + 4);
	pendingShutdown = true;
	log("NS::RebootSystem (timeout = %llu)\n", timeout);

	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::terminateTitle(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("NS::TerminateTitle (title ID = %llX)\n", titleID);

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::setApplicationCpuTimeLimit(u32 messagePointer) {
	const u32 percent = mem.read32(messagePointer + 8);
	log("NS::SetApplicationCpuTimeLimit (percent = %u)\n", percent);

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::launchApplication(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("NS::LaunchApplication (title ID = %llX)\n", titleID);

	mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NSService::rebootSystemClean(u32 messagePointer) {
	pendingShutdown = true;
	log("NS::RebootSystemClean\n");

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
