#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>

class NDMService {
	using Handle = HorizonHandle;
	enum class ExclusiveState : u32 {
		None = 0,
		Infrastructure = 1,
		LocalComms = 2,
		StreetPass = 3,
		StreetPassData = 4,
	};
	enum class DaemonStatus : u32 {
		Busy = 0,
		Idle = 1,
		Suspended = 2,
	};

	Handle handle = KernelHandles::NDM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, ndmLogger)

	// Service commands
	void clearHalfAwakeMacFilter(u32 messagePointer);
	void enterExclusiveState(u32 messagePointer);
	void exitExclusiveState(u32 messagePointer);
	void overrideDefaultDaemons(u32 messagePointer);
	void queryExclusiveState(u32 messagePointer);
	void resumeDaemons(u32 messagePointer);
	void resumeScheduler(u32 messagePointer);
	void suspendDaemons(u32 messagePointer);
	void suspendScheduler(u32 messagePointer);
	void lockState(u32 messagePointer);
	void unlockState(u32 messagePointer);
	void queryStatus(u32 messagePointer);
	void getDaemonDisableCount(u32 messagePointer);
	void getSchedulerDisableCount(u32 messagePointer);
	void setScanInterval(u32 messagePointer);
	void getScanInterval(u32 messagePointer);
	void setRetryInterval(u32 messagePointer);
	void getRetryInterval(u32 messagePointer);
	void resetDefaultDaemons(u32 messagePointer);
	void getDefaultDaemons(u32 messagePointer);

	ExclusiveState exclusiveState = ExclusiveState::None;
	u32 daemonMask = 0xF;
	u32 defaultDaemonMask = 0xF;
	std::array<u32, 4> daemonSuspendCount {};
	std::array<DaemonStatus, 4> daemonStatuses { DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle };
	u32 schedulerDisableCount = 0;
	u32 scanInterval = 0;
	u32 retryInterval = 0;
	bool stateLocked = false;

  public:
	NDMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
