#include "../../../include/services/ns.hpp"
#include <algorithm>
#include "../../../include/ipc.hpp"

namespace NSCommands {
	enum : u32 {
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
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case NSCommands::SetWirelessRebootInfo: {
			log("NS::SetWirelessRebootInfo\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case NSCommands::ShutdownAsync: {
			log("NS::ShutdownAsync\n");
			pendingShutdown = true;
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case NSCommands::RebootSystem: {
			log("NS::RebootSystem\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case NSCommands::LaunchApplication: {
			log("NS::LaunchApplication\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("NS service requested unknown command: %08X for type %d\n", command, static_cast<int>(type));
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

// Stub implementation for other functions
void NSService::launchTitle(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::launchFIRM(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::terminateApplication(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::terminateProcess(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::launchApplicationFIRM(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::setWirelessRebootInfo(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::cardUpdateInitialize(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::cardUpdateShutdown(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::shutdownAsync(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::rebootSystem(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::terminateTitle(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::setApplicationCpuTimeLimit(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::launchApplication(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
void NSService::rebootSystemClean(u32 messagePointer) { mem.write32(messagePointer + 4, Result::Success); }
