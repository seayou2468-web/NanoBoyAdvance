#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>
#include <unordered_map>

class PMService {
	using Handle = HorizonHandle;

	Handle appHandle = KernelHandles::PM_APP;
	Handle dbgHandle = KernelHandles::PM_DBG;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, srvLogger)
	u32 appCpuTimeLimit = 30;
	std::unordered_map<u32, u64> appResourceLimits;
	bool titleRunning = false;
	u64 runningTitleID = 0;
	std::array<u8, 0x100> firmLaunchParams {};
	u32 lastLaunchFlags = 0;

	void launchTitle(u32 messagePointer);
	void launchFirm(u32 messagePointer);
	void terminateApplication(u32 messagePointer);
	void terminateTitle(u32 messagePointer);
	void terminateProcess(u32 messagePointer);
	void prepareForReboot(u32 messagePointer);
	void getFirmLaunchParams(u32 messagePointer);
	void getTitleExheaderFlags(u32 messagePointer);
	void setFirmLaunchParams(u32 messagePointer);
	void unregisterProcess(u32 messagePointer);
	void launchTitleUpdate(u32 messagePointer);
	void launchAppDebug(u32 messagePointer);
	void launchApp(u32 messagePointer);
	void runQueuedProcess(u32 messagePointer);
	void setAppResourceLimit(u32 messagePointer);
	void getAppResourceLimit(u32 messagePointer);

  public:
	enum class Type : u8 {
		App,
		Debug,
	};

	PMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};
