#include "../../../include/services/ptm.hpp"
#include "../../../include/ipc.hpp"
#include <algorithm>
#include <chrono>

namespace PTMCommands {
	enum : u32 {
		SetSystemTime = 0x00010040,
		GetAdapterState = 0x00050000,
		GetShellState = 0x00060000,
		GetBatteryLevel = 0x00070000,
		GetBatteryChargeState = 0x00080000,
		GetPedometerState = 0x00090000,
		GetStepHistory = 0x000B00C2,
		GetTotalStepCount = 0x000C0000,
		GetStepHistoryAll = 0x000F0084,

		ReplySleepQuery = 0x04020040,
		CheckNew3DS = 0x040A0000,

		GetPlayHistory = 0x08070082,
		GetPlayHistoryStart = 0x08080000,
		GetPlayHistoryLength = 0x08090000,
		CalcPlayHistoryStart = 0x080B0080,
		GetSoftwareClosedFlag = 0x080F0000,
		ClearSoftwareClosedFlag = 0x08100000,
		ConfigureNew3DSCPU = 0x08180040,

		// ptm:gets functions
		GetSystemTime = 0x04010000,
	};
}

void PTMService::reset() {
	shellOpen = true;
	batteryCharging = config.chargerPlugged && (config.batteryPercentage < 100);
	pedometerCounting = true;
}

void PTMService::handleSyncRequest(u32 messagePointer, PTMService::Type type) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case PTMCommands::GetAdapterState: getAdapterState(messagePointer); break;
		case PTMCommands::GetShellState: getShellState(messagePointer); break;
		case PTMCommands::GetBatteryLevel: getBatteryLevel(messagePointer); break;
		case PTMCommands::GetBatteryChargeState: getBatteryChargeState(messagePointer); break;
		case PTMCommands::GetPedometerState: getPedometerState(messagePointer); break;
		case PTMCommands::GetStepHistory: getStepHistory(messagePointer); break;
		case PTMCommands::GetTotalStepCount: getTotalStepCount(messagePointer); break;
		case PTMCommands::GetStepHistoryAll: getStepHistoryAll(messagePointer); break;
		case PTMCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case PTMCommands::ConfigureNew3DSCPU: configureNew3DSCPU(messagePointer); break;
		case PTMCommands::GetSystemTime: getSystemTime(messagePointer); break;
		case PTMCommands::SetSystemTime: setSystemTime(messagePointer); break;
		case PTMCommands::GetPlayHistory: getPlayHistory(messagePointer); break;
		case PTMCommands::GetPlayHistoryStart: getPlayHistoryStart(messagePointer); break;
		case PTMCommands::GetPlayHistoryLength: getPlayHistoryLength(messagePointer); break;
		case PTMCommands::CalcPlayHistoryStart: calcPlayHistoryStart(messagePointer); break;
		case PTMCommands::GetSoftwareClosedFlag: getSoftwareClosedFlag(messagePointer); break;
		case PTMCommands::ClearSoftwareClosedFlag: clearSoftwareClosedFlag(messagePointer); break;
		case PTMCommands::ReplySleepQuery: {
			log("PTM::ReplySleepQuery\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		default:
			log("PTM service requested unknown command: %08X for type %d\n", command, static_cast<int>(type));
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void PTMService::getAdapterState(u32 messagePointer) {
	log("PTM::GetAdapterState\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(config.chargerPlugged ? 1u : 0u);
}

void PTMService::getShellState(u32 messagePointer) {
	log("PTM::GetShellState\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(shellOpen ? 1u : 0u);
}

void PTMService::getBatteryLevel(u32 messagePointer) {
	log("PTM::GetBatteryLevel\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(static_cast<u32>(batteryPercentToLevel(config.batteryPercentage)));
}

void PTMService::getBatteryChargeState(u32 messagePointer) {
	log("PTM::GetBatteryChargeState\n");
	IPC::RequestParser rp(messagePointer, mem);
	bool charging = config.chargerPlugged && (config.batteryPercentage < 100);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(charging ? 1u : 0u);
}

void PTMService::getPedometerState(u32 messagePointer) {
	log("PTM::GetPedometerState\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(pedometerCounting ? 1u : 0u);
}

void PTMService::getStepHistory(u32 messagePointer) {
	log("PTM::GetStepHistory\n");
	IPC::RequestParser rp(messagePointer, mem);
	u32 hours = rp.Pop();
	u64 startTime = rp.Pop64();
	// Reference fills buffer with steps_per_hour. We'll just return success for now.
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}

void PTMService::getTotalStepCount(u32 messagePointer) {
	log("PTM::GetTotalStepCount\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1337u); // Return a constant step count
}

void PTMService::getStepHistoryAll(u32 messagePointer) {
	log("PTM::GetStepHistoryAll\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}

void PTMService::checkNew3DS(u32 messagePointer) {
	log("PTM::CheckNew3DS\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(config.isNew3DS ? 1u : 0u);
}

void PTMService::configureNew3DSCPU(u32 messagePointer) {
	log("PTM::ConfigureNew3DSCPU\n");
	IPC::RequestParser rp(messagePointer, mem);
	u32 value = rp.Pop();
	// Reference allows L2 cache and higher clock if value & 1 and value & 2.
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void PTMService::getSystemTime(u32 messagePointer) {
	log("PTM::GetSystemTime\n");
	IPC::RequestParser rp(messagePointer, mem);

	using namespace std::chrono;
	const auto now = system_clock::now();
	const auto msSinceUnix = duration_cast<milliseconds>(now.time_since_epoch()).count();
	constexpr s64 unixTo2000Ms = 946684800ll * 1000ll;
	const s64 since2000 = std::max<s64>(0, msSinceUnix - unixTo2000Ms);

	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(static_cast<u64>(since2000));
}

void PTMService::setSystemTime(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u64 newTime = rp.Pop64();
	log("PTM::SetSystemTime (new time = %llu)\n", newTime);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void PTMService::getPlayHistory(u32 messagePointer) {
	log("PTM::GetPlayHistory\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}

void PTMService::getPlayHistoryStart(u32 messagePointer) {
	log("PTM::GetPlayHistoryStart\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(0ULL);
}

void PTMService::getPlayHistoryLength(u32 messagePointer) {
	log("PTM::GetPlayHistoryLength\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(0ULL);
}

void PTMService::calcPlayHistoryStart(u32 messagePointer) {
	log("PTM::CalcPlayHistoryStart\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(0ULL);
}

void PTMService::getSoftwareClosedFlag(u32 messagePointer) {
	log("PTM::GetSoftwareClosedFlag\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0u);
}

void PTMService::clearSoftwareClosedFlag(u32 messagePointer) {
	log("PTM::ClearSoftwareClosedFlag\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
