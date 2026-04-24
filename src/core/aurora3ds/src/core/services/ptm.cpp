#include "../../../include/services/ptm.hpp"

#include "../../../include/ipc.hpp"
#include <algorithm>
#include <chrono>

namespace PTMCommands {
	enum : u32 {
		GetAdapterState = 0x00050000,
		GetShellState = 0x00060000,
		GetBatteryLevel = 0x00070000,
		GetBatteryChargeState = 0x00080000,
		GetPedometerState = 0x00090000,
		GetStepHistory = 0x000B00C2,
		GetTotalStepCount = 0x000C0000,
		GetStepHistoryAll = 0x000F0084,
		ConfigureNew3DSCPU = 0x08180040,

		// ptm:gets functions
		GetSystemTime = 0x04010000,
		SetSystemTime = 0x00010040,

		// ptm:play functions
		GetPlayHistory = 0x08070082,
		GetPlayHistoryStart = 0x08080000,
		GetPlayHistoryLength = 0x08090000,
		CalcPlayHistoryStart = 0x080B0080,
		GetSoftwareClosedFlag = 0x080F0000,
		ClearSoftwareClosedFlag = 0x08100000,
	};
}

u64 PTMService::getCurrentSystemTimeMs() const {
	using namespace std::chrono;
	const auto now = system_clock::now();
	const auto msSinceUnix = duration_cast<milliseconds>(now.time_since_epoch()).count();
	constexpr s64 unixTo2000Ms = 946684800ll * 1000ll;  // 1970-01-01 -> 2000-01-01
	const s64 since2000 = std::max<s64>(0, msSinceUnix - unixTo2000Ms);
	return static_cast<u64>(since2000);
}

void PTMService::reset() {
	systemTimeMsSince2000 = getCurrentSystemTimeMs();
	totalStepCount = 0;
	cpuConfig = 0;
	softwareClosedFlag = false;
}

void PTMService::handleSyncRequest(u32 messagePointer, PTMService::Type type) {
	const u32 command = mem.read32(messagePointer);

	// ptm:play functions
	switch (command) {
		case PTMCommands::ConfigureNew3DSCPU: configureNew3DSCPU(messagePointer); break;
		case PTMCommands::GetAdapterState: getAdapterState(messagePointer); break;
		case PTMCommands::GetBatteryChargeState: getBatteryChargeState(messagePointer); break;
		case PTMCommands::GetBatteryLevel: getBatteryLevel(messagePointer); break;
		case PTMCommands::GetShellState: getShellState(messagePointer); break;
		case PTMCommands::GetPedometerState: getPedometerState(messagePointer); break;
		case PTMCommands::GetStepHistory: getStepHistory(messagePointer); break;
		case PTMCommands::GetStepHistoryAll: getStepHistoryAll(messagePointer); break;
		case PTMCommands::GetTotalStepCount: getTotalStepCount(messagePointer); break;

		default:
			// ptm:play-only functions
			if (type == Type::PLAY) {
				switch (command) {
					case PTMCommands::GetPlayHistory:
					case PTMCommands::GetPlayHistoryStart:
					case PTMCommands::GetPlayHistoryLength:
					case PTMCommands::CalcPlayHistoryStart:
						mem.write32(messagePointer, IPC::responseHeader(command >> 16, 3, 0));
						mem.write32(messagePointer + 4, Result::Success);
						mem.write64(messagePointer + 8, 0);
						log("PTM::PLAY compatibility command %08X\n", command);
						break;

					default: Helpers::panic("PTM PLAY service requested. Command: %08X\n", command); break;
				}
			} else if (type == Type::GETS) {
				switch (command) {
					case PTMCommands::GetSystemTime: getSystemTime(messagePointer); break;

					default: Helpers::panic("PTM GETS service requested. Command: %08X\n", command); break;
				}
			} else if (type == Type::SETS) {
				switch (command) {
					case PTMCommands::SetSystemTime: setSystemTime(messagePointer); break;
					default: Helpers::panic("PTM SETS service requested. Command: %08X\n", command); break;
				}
			} else if (type == Type::SYSM) {
				switch (command) {
					case PTMCommands::GetSoftwareClosedFlag: getSoftwareClosedFlag(messagePointer); break;
					case PTMCommands::ClearSoftwareClosedFlag: clearSoftwareClosedFlag(messagePointer); break;

					default:
						mem.write32(messagePointer + 4, Result::Success);
						Helpers::warn("PTM SYSM service requested. Command: %08X\n", command);
						break;
				}
			} else {
				Helpers::panic("PTM service requested. Command: %08X\n", command);
			}
	}
}

void PTMService::getShellState(u32 messagePointer) {
	log("PTM::GetShellState\n");

	mem.write32(messagePointer, IPC::responseHeader(0x6, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// Aurora3DS currently does not emulate shell-open/close sensors. Assume shell open.
	mem.write8(messagePointer + 8, 1);
}

void PTMService::getAdapterState(u32 messagePointer) {
	log("PTM::GetAdapterState\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, config.chargerPlugged ? 1 : 0);
}

void PTMService::getBatteryChargeState(u32 messagePointer) {
	log("PTM::GetBatteryChargeState");
	// We're only charging if the battery is not already full
	const bool charging = config.chargerPlugged && (config.batteryPercentage < 100);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, charging ? 1 : 0);
}

void PTMService::getPedometerState(u32 messagePointer) {
	log("PTM::GetPedometerState");
	constexpr bool countingSteps = true;

	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, countingSteps ? 1 : 0);
}

void PTMService::getBatteryLevel(u32 messagePointer) {
	log("PTM::GetBatteryLevel");

	mem.write32(messagePointer, IPC::responseHeader(0x7, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, batteryPercentToLevel(config.batteryPercentage));
}

void PTMService::getStepHistory(u32 messagePointer) {
	log("PTM::GetStepHistory\n");
	const u32 outBuffer = mem.read32(messagePointer + 0x100 + 4);
	const u32 requestedBytes = mem.read32(messagePointer + 8);
	const u32 entryCount = std::min<u32>(requestedBytes / sizeof(u16), 30);
	const u16 dailyBase = static_cast<u16>(totalStepCount / 30);
	for (u32 i = 0; i < entryCount; i++) {
		mem.write16(outBuffer + i * sizeof(u16), static_cast<u16>(dailyBase + (i % 3)));
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getStepHistoryAll(u32 messagePointer) {
	log("PTM::GetStepHistoryAll\n");
	const u32 outBuffer = mem.read32(messagePointer + 0x100 + 4);
	const u32 requestedBytes = mem.read32(messagePointer + 8);
	const u32 entryCount = std::min<u32>(requestedBytes / sizeof(u16), 7 * 30);
	const u16 dailyBase = static_cast<u16>(totalStepCount / std::max<u32>(1, entryCount));
	for (u32 i = 0; i < entryCount; i++) {
		mem.write16(outBuffer + i * sizeof(u16), static_cast<u16>(dailyBase + (i % 5)));
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getTotalStepCount(u32 messagePointer) {
	log("PTM::GetTotalStepCount\n");
	// Simulate basic progression while preserving monotonicity.
	totalStepCount += 1;
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, totalStepCount);
}

void PTMService::configureNew3DSCPU(u32 messagePointer) {
	cpuConfig = static_cast<u8>(mem.read32(messagePointer + 4) & 0xFF);
	log("PTM::ConfigureNew3DSCPU (config = %u)\n", cpuConfig);
	mem.write32(messagePointer, IPC::responseHeader(0x818, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getSystemTime(u32 messagePointer) {
	log("PTM::GetSystemTime\n");

	mem.write32(messagePointer, IPC::responseHeader(0x401, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, systemTimeMsSince2000);
}

void PTMService::getSoftwareClosedFlag(u32 messagePointer) {
	log("PTM::GetSoftwareClosedFlag\n");
	mem.write32(messagePointer, IPC::responseHeader(0x80F, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, softwareClosedFlag ? 1 : 0);
}

void PTMService::clearSoftwareClosedFlag(u32 messagePointer) {
	log("PTM::ClearSoftwareClosedFlag\n");
	softwareClosedFlag = false;
	mem.write32(messagePointer, IPC::responseHeader(0x810, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::setSystemTime(u32 messagePointer) {
	systemTimeMsSince2000 = mem.read64(messagePointer + 4);
	log("PTM::SetSystemTime (new time = %llu)\n", systemTimeMsSince2000);
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
