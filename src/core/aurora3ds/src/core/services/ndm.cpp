#include "../../../include/services/ndm.hpp"

#include "../../../include/ipc.hpp"

namespace NDMCommands {
	enum : u32 {
		EnterExclusiveState = 0x00010042,
		ExitExclusiveState = 0x00020002,
		QueryExclusiveMode = 0x00030000,
		LockState = 0x00040002,
		UnlockState = 0x00050002,
		OverrideDefaultDaemons = 0x00140040,
		ResetDefaultDaemons = 0x00150000,
		GetDefaultDaemons = 0x00160000,
		SuspendDaemons = 0x00060040,
		ResumeDaemons = 0x00070040,
		SuspendScheduler = 0x00080040,
		ResumeScheduler = 0x00090000,
		QueryStatus = 0x000D0040,
		GetDaemonDisableCount = 0x000E0040,
		GetSchedulerDisableCount = 0x000F0000,
		SetScanInterval = 0x00100040,
		GetScanInterval = 0x00110000,
		SetRetryInterval = 0x00120040,
		GetRetryInterval = 0x00130000,
		ClearHalfAwakeMacFilter = 0x00170000,
	};
}

void NDMService::reset() {
	exclusiveState = ExclusiveState::None;
	daemonMask = 0xF;
	defaultDaemonMask = 0xF;
	daemonSuspendCount.fill(0);
	daemonStatuses.fill(DaemonStatus::Idle);
	schedulerDisableCount = 0;
	scanInterval = 0;
	retryInterval = 0;
	stateLocked = false;
}

void NDMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NDMCommands::EnterExclusiveState: enterExclusiveState(messagePointer); break;
		case NDMCommands::ExitExclusiveState: exitExclusiveState(messagePointer); break;
		case NDMCommands::LockState: lockState(messagePointer); break;
		case NDMCommands::UnlockState: unlockState(messagePointer); break;
		case NDMCommands::ClearHalfAwakeMacFilter: clearHalfAwakeMacFilter(messagePointer); break;
		case NDMCommands::OverrideDefaultDaemons: overrideDefaultDaemons(messagePointer); break;
		case NDMCommands::ResetDefaultDaemons: resetDefaultDaemons(messagePointer); break;
		case NDMCommands::GetDefaultDaemons: getDefaultDaemons(messagePointer); break;
		case NDMCommands::QueryExclusiveMode: queryExclusiveState(messagePointer); break;
		case NDMCommands::QueryStatus: queryStatus(messagePointer); break;
		case NDMCommands::GetDaemonDisableCount: getDaemonDisableCount(messagePointer); break;
		case NDMCommands::GetSchedulerDisableCount: getSchedulerDisableCount(messagePointer); break;
		case NDMCommands::SetScanInterval: setScanInterval(messagePointer); break;
		case NDMCommands::GetScanInterval: getScanInterval(messagePointer); break;
		case NDMCommands::SetRetryInterval: setRetryInterval(messagePointer); break;
		case NDMCommands::GetRetryInterval: getRetryInterval(messagePointer); break;
		case NDMCommands::ResumeDaemons: resumeDaemons(messagePointer); break;
		case NDMCommands::ResumeScheduler: resumeScheduler(messagePointer); break;
		case NDMCommands::SuspendDaemons: suspendDaemons(messagePointer); break;
		case NDMCommands::SuspendScheduler: suspendScheduler(messagePointer); break;
		default: Helpers::panic("NDM service requested. Command: %08X\n", command);
	}
}

void NDMService::enterExclusiveState(u32 messagePointer) {
	log("NDM::EnterExclusiveState (stubbed)\n");
	const u32 state = mem.read32(messagePointer + 4);

	// Check that the exclusive state config is valid
	if (state > 4) {
		Helpers::warn("NDM::EnterExclusiveState: Invalid state %d", state);
	} else {
		exclusiveState = static_cast<ExclusiveState>(state);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::exitExclusiveState(u32 messagePointer) {
	log("NDM::ExitExclusiveState (stubbed)\n");
	exclusiveState = ExclusiveState::None;

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::queryExclusiveState(u32 messagePointer) {
	log("NDM::QueryExclusiveState\n");

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(exclusiveState));
}

void NDMService::overrideDefaultDaemons(u32 messagePointer) {
	const u32 bitMask = mem.read32(messagePointer + 4) & 0xF;
	log("NDM::OverrideDefaultDaemons (mask=%X)\n", bitMask);
	defaultDaemonMask = bitMask;
	daemonMask = bitMask;
	for (u32 i = 0; i < daemonStatuses.size(); i++) {
		daemonStatuses[i] = (bitMask & (1u << i)) ? DaemonStatus::Idle : DaemonStatus::Busy;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::resumeDaemons(u32 messagePointer) {
	const u32 bitMask = mem.read32(messagePointer + 4) & 0xF;
	log("NDM::resumeDaemons (mask=%X)\n", bitMask);

	for (u32 i = 0; i < daemonStatuses.size(); i++) {
		if ((bitMask & (1u << i)) == 0) {
			continue;
		}
		if (daemonSuspendCount[i] == 0) {
			mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
			mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
			return;
		}
	}

	for (u32 i = 0; i < daemonStatuses.size(); i++) {
		if ((bitMask & (1u << i)) == 0) {
			continue;
		}
		daemonSuspendCount[i]--;
		if (daemonSuspendCount[i] == 0) {
			daemonStatuses[i] = DaemonStatus::Idle;
		}
	}
	daemonMask &= ~bitMask;

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::suspendDaemons(u32 messagePointer) {
	const u32 bitMask = mem.read32(messagePointer + 4) & 0xF;
	log("NDM::SuspendDaemons (mask=%X)\n", bitMask);

	daemonMask = defaultDaemonMask & ~bitMask;
	for (u32 i = 0; i < daemonStatuses.size(); i++) {
		if ((bitMask & (1u << i)) == 0) {
			continue;
		}
		daemonSuspendCount[i]++;
		daemonStatuses[i] = DaemonStatus::Suspended;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::resumeScheduler(u32 messagePointer) {
	log("NDM::ResumeScheduler\n");
	if (schedulerDisableCount > 0) {
		schedulerDisableCount--;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::suspendScheduler(u32 messagePointer) {
	const u32 runInBackground = mem.read32(messagePointer + 4);
	log("NDM::SuspendScheduler (runInBackground=%u)\n", runInBackground);
	schedulerDisableCount++;

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::clearHalfAwakeMacFilter(u32 messagePointer) {
	log("NDM::ClearHalfAwakeMacFilter (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::lockState(u32 messagePointer) {
	log("NDM::LockState\n");
	stateLocked = true;
	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::unlockState(u32 messagePointer) {
	log("NDM::UnlockState\n");
	stateLocked = false;
	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::queryStatus(u32 messagePointer) {
	const u32 daemon = mem.read8(messagePointer + 4);
	log("NDM::QueryStatus daemon=%u\n", daemon);

	if (daemon >= daemonStatuses.size()) {
		mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		return;
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(daemonStatuses[daemon]));
}

void NDMService::getDaemonDisableCount(u32 messagePointer) {
	const u32 daemon = mem.read8(messagePointer + 4);
	if (daemon >= daemonSuspendCount.size()) {
		mem.write32(messagePointer, IPC::responseHeader(0xE, 3, 0));
		mem.write32(messagePointer + 4, Result::OS::InvalidCombination);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, 0);
		return;
	}

	const u32 count = daemonSuspendCount[daemon];
	log("NDM::GetDaemonDisableCount daemon=%u count=%u\n", daemon, count);
	mem.write32(messagePointer, IPC::responseHeader(0xE, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, count);
	mem.write32(messagePointer + 12, count);
}

void NDMService::getSchedulerDisableCount(u32 messagePointer) {
	log("NDM::GetSchedulerDisableCount count=%u\n", schedulerDisableCount);
	mem.write32(messagePointer, IPC::responseHeader(0xF, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, schedulerDisableCount);
	mem.write32(messagePointer + 12, schedulerDisableCount);
}

void NDMService::setScanInterval(u32 messagePointer) {
	scanInterval = mem.read32(messagePointer + 4);
	log("NDM::SetScanInterval %u\n", scanInterval);
	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::getScanInterval(u32 messagePointer) {
	log("NDM::GetScanInterval %u\n", scanInterval);
	mem.write32(messagePointer, IPC::responseHeader(0x11, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, scanInterval);
}

void NDMService::setRetryInterval(u32 messagePointer) {
	retryInterval = mem.read32(messagePointer + 4);
	log("NDM::SetRetryInterval %u\n", retryInterval);
	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::getRetryInterval(u32 messagePointer) {
	log("NDM::GetRetryInterval %u\n", retryInterval);
	mem.write32(messagePointer, IPC::responseHeader(0x13, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, retryInterval);
}

void NDMService::resetDefaultDaemons(u32 messagePointer) {
	log("NDM::ResetDefaultDaemons\n");
	defaultDaemonMask = 0xF;
	daemonMask = defaultDaemonMask;
	for (u32 i = 0; i < daemonStatuses.size(); i++) {
		daemonStatuses[i] = DaemonStatus::Idle;
	}
	mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::getDefaultDaemons(u32 messagePointer) {
	log("NDM::GetDefaultDaemons mask=%X\n", defaultDaemonMask);
	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, defaultDaemonMask);
}
