#pragma once

#include "../helpers.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include "../config.hpp"

class PTMService {
	using Handle = HorizonHandle;

	Memory& mem;
	const EmulatorConfig& config;
	MAKE_LOG_FUNCTION(log, ptmLogger)

	// Internal state
	bool shellOpen = true;
	bool batteryCharging = false;
	bool pedometerCounting = true;

  public:
	enum class Type {
		U,
		SYSM,
		PLAY,
		GETS,
		SETS,
	};

	PTMService(Memory& mem, const EmulatorConfig& config) : mem(mem), config(config) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);

  private:
	// Service commands
	void getAdapterState(u32 messagePointer);
	void getShellState(u32 messagePointer);
	void getBatteryLevel(u32 messagePointer);
	void getBatteryChargeState(u32 messagePointer);
	void getPedometerState(u32 messagePointer);
	void getStepHistory(u32 messagePointer);
	void getTotalStepCount(u32 messagePointer);
	void getStepHistoryAll(u32 messagePointer);
	void configureNew3DSCPU(u32 messagePointer);
	void getSystemTime(u32 messagePointer);
	void setSystemTime(u32 messagePointer);
	void getSoftwareClosedFlag(u32 messagePointer);
	void clearSoftwareClosedFlag(u32 messagePointer);

	// New ones from reference
	void checkNew3DS(u32 messagePointer);
	void getPlayHistory(u32 messagePointer);
	void getPlayHistoryStart(u32 messagePointer);
	void getPlayHistoryLength(u32 messagePointer);
	void calcPlayHistoryStart(u32 messagePointer);

	static constexpr u8 batteryPercentToLevel(u32 percentage) {
		if (percentage > 80) return 5;
		if (percentage > 60) return 4;
		if (percentage > 40) return 3;
		if (percentage > 20) return 2;
		if (percentage > 5) return 1;
		return 0;
	}
};
