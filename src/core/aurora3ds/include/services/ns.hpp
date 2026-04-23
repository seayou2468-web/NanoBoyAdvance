#pragma once
#include <array>

#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class NSService {
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nsLogger)
	static constexpr u32 WirelessRebootInfoBufferSize = 0x20;
	std::array<u8, WirelessRebootInfoBufferSize> wirelessRebootInfo{};
	bool pendingShutdown = false;

	// Service commands
	void launchTitle(u32 messagePointer);
	void launchFIRM(u32 messagePointer);
	void terminateApplication(u32 messagePointer);
	void terminateProcess(u32 messagePointer);
	void launchApplicationFIRM(u32 messagePointer);
	void setWirelessRebootInfo(u32 messagePointer);
	void cardUpdateInitialize(u32 messagePointer);
	void cardUpdateShutdown(u32 messagePointer);
	void shutdownAsync(u32 messagePointer);
	void rebootSystem(u32 messagePointer);
	void terminateTitle(u32 messagePointer);
	void setApplicationCpuTimeLimit(u32 messagePointer);
	void launchApplication(u32 messagePointer);
	void rebootSystemClean(u32 messagePointer);

  public:
	enum class Type {
		S,  // ns:s
		P,  // ns:p
		C,  // ns:c
	};

	NSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};
