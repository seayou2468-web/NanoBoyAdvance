#include "../../../include/services/pm.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"

namespace PMCommands {
	enum : u32 {
		LaunchTitle = 0x0001,
		LaunchFirm = 0x0002,
		TerminateApplication = 0x0003,
		TerminateTitle = 0x0004,
		TerminateProcess = 0x0005,
		PrepareForReboot = 0x0006,
		GetFirmLaunchParams = 0x0007,
		GetTitleExheaderFlags = 0x0008,
		SetFirmLaunchParams = 0x0009,
		SetAppResourceLimit = 0x000A,
		GetAppResourceLimit = 0x000B,
		UnregisterProcess = 0x000C,
		LaunchTitleUpdate = 0x000D,

		LaunchAppDebug = 0x0001,
		LaunchApp = 0x0002,
		RunQueuedProcess = 0x0003,
	};
}

void PMService::reset() {
	appCpuTimeLimit = 30;
	titleRunning = false;
	runningTitleID = 0;
	lastLaunchFlags = 0;
	firmLaunchParams.fill(0);
}

void PMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	if (type == Type::App) {
		switch (commandID) {
			case PMCommands::LaunchTitle: launchTitle(messagePointer); return;
			case PMCommands::LaunchFirm: launchFirm(messagePointer); return;
			case PMCommands::TerminateApplication: terminateApplication(messagePointer); return;
			case PMCommands::TerminateTitle: terminateTitle(messagePointer); return;
			case PMCommands::TerminateProcess: terminateProcess(messagePointer); return;
			case PMCommands::PrepareForReboot: prepareForReboot(messagePointer); return;
			case PMCommands::GetFirmLaunchParams: getFirmLaunchParams(messagePointer); return;
			case PMCommands::GetTitleExheaderFlags: getTitleExheaderFlags(messagePointer); return;
			case PMCommands::SetFirmLaunchParams: setFirmLaunchParams(messagePointer); return;
			case PMCommands::SetAppResourceLimit: setAppResourceLimit(messagePointer); return;
			case PMCommands::GetAppResourceLimit: getAppResourceLimit(messagePointer); return;
			case PMCommands::UnregisterProcess: unregisterProcess(messagePointer); return;
			case PMCommands::LaunchTitleUpdate: launchTitleUpdate(messagePointer); return;
			default: break;
		}
	}
	if (type == Type::Debug) {
		switch (commandID) {
			case PMCommands::LaunchAppDebug: launchAppDebug(messagePointer); return;
			case PMCommands::LaunchApp: launchApp(messagePointer); return;
			case PMCommands::RunQueuedProcess: runQueuedProcess(messagePointer); return;
			default: break;
		}
	}

	log("pm:%s command %08X [default-success]\n", type == Type::App ? "app" : "dbg", command);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::launchTitle(u32 messagePointer) {
	const u32 mediaType = mem.read32(messagePointer + 4);
	const u64 titleID = mem.read64(messagePointer + 8);
	lastLaunchFlags = mem.read32(messagePointer + 16);
	log("pm:app::LaunchTitle(media=%u, titleID=%016llX, flags=%08X)\n", mediaType, titleID, lastLaunchFlags);

	titleRunning = true;
	runningTitleID = titleID;
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::LaunchTitle, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::launchFirm(u32 messagePointer) {
	const u32 firmTidLow = mem.read32(messagePointer + 4);
	const u32 size = std::min<u32>(mem.read32(messagePointer + 8), static_cast<u32>(firmLaunchParams.size()));
	const u32 src = mem.read32(messagePointer + 16);
	log("pm:app::LaunchFirm(tidLow=%08X, size=%u, src=%08X)\n", firmTidLow, size, src);

	for (u32 i = 0; i < size; i++) {
		firmLaunchParams[i] = mem.read8(src + i);
	}

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::LaunchFirm, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::terminateApplication(u32 messagePointer) {
	log("pm:app::TerminateApplication(running=%d)\n", titleRunning ? 1 : 0);
	titleRunning = false;
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::TerminateApplication, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::terminateTitle(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("pm:app::TerminateTitle(%016llX)\n", titleID);
	if (runningTitleID == titleID) {
		titleRunning = false;
	}
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::TerminateTitle, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::terminateProcess(u32 messagePointer) {
	const u32 pid = mem.read32(messagePointer + 4);
	log("pm:app::TerminateProcess(pid=%u)\n", pid);
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::TerminateProcess, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::prepareForReboot(u32 messagePointer) {
	log("pm:app::PrepareForReboot\n");
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::PrepareForReboot, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::getFirmLaunchParams(u32 messagePointer) {
	const u32 size = std::min<u32>(mem.read32(messagePointer + 4), static_cast<u32>(firmLaunchParams.size()));
	const u32 out = mem.read32(messagePointer + 12);
	log("pm:app::GetFirmLaunchParams(size=%u, out=%08X)\n", size, out);

	for (u32 i = 0; i < size; i++) {
		mem.write8(out + i, firmLaunchParams[i]);
	}

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::GetFirmLaunchParams, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, size);
}

void PMService::getTitleExheaderFlags(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("pm:app::GetTitleExheaderFlags(titleID=%016llX)\n", titleID);
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::GetTitleExheaderFlags, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void PMService::setFirmLaunchParams(u32 messagePointer) {
	const u32 size = std::min<u32>(mem.read32(messagePointer + 4), static_cast<u32>(firmLaunchParams.size()));
	const u32 src = mem.read32(messagePointer + 12);
	log("pm:app::SetFirmLaunchParams(size=%u, src=%08X)\n", size, src);
	for (u32 i = 0; i < size; i++) {
		firmLaunchParams[i] = mem.read8(src + i);
	}
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::SetFirmLaunchParams, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::setAppResourceLimit(u32 messagePointer) {
	constexpr u32 CpuTimeResourceType = 0;

	const u32 resourceType = mem.read32(messagePointer + 8);
	const u32 value = mem.read32(messagePointer + 12);
	log("pm:app::SetAppResourceLimit(type=%u, value=%u)\n", resourceType, value);

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::SetAppResourceLimit, 1, 0));
	if (resourceType != CpuTimeResourceType) {
		mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		return;
	}

	appCpuTimeLimit = value;
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::unregisterProcess(u32 messagePointer) {
	const u32 pid = mem.read32(messagePointer + 4);
	log("pm:app::UnregisterProcess(pid=%u)\n", pid);
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::UnregisterProcess, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::launchTitleUpdate(u32 messagePointer) {
	const u32 mediaType = mem.read32(messagePointer + 4);
	const u64 titleID = mem.read64(messagePointer + 8);
	lastLaunchFlags = mem.read32(messagePointer + 16);
	log("pm:app::LaunchTitleUpdate(media=%u, titleID=%016llX, flags=%08X)\n", mediaType, titleID, lastLaunchFlags);

	titleRunning = true;
	runningTitleID = titleID;
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::LaunchTitleUpdate, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::launchAppDebug(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("pm:dbg::LaunchAppDebug(titleID=%016llX)\n", titleID);
	titleRunning = true;
	runningTitleID = titleID;
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::LaunchAppDebug, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::launchApp(u32 messagePointer) {
	const u64 titleID = mem.read64(messagePointer + 4);
	log("pm:dbg::LaunchApp(titleID=%016llX)\n", titleID);
	titleRunning = true;
	runningTitleID = titleID;
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::LaunchApp, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::runQueuedProcess(u32 messagePointer) {
	log("pm:dbg::RunQueuedProcess\n");
	mem.write32(messagePointer, IPC::responseHeader(PMCommands::RunQueuedProcess, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PMService::getAppResourceLimit(u32 messagePointer) {
	constexpr u32 CpuTimeResourceType = 0;

	const u32 resourceType = mem.read32(messagePointer + 8);
	log("pm:app::GetAppResourceLimit(type=%u)\n", resourceType);

	mem.write32(messagePointer, IPC::responseHeader(PMCommands::GetAppResourceLimit, 3, 0));
	if (resourceType != CpuTimeResourceType) {
		mem.write32(messagePointer + 4, Result::OS::NotImplemented);
		mem.write64(messagePointer + 8, 0);
		return;
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, static_cast<u64>(appCpuTimeLimit));
}
