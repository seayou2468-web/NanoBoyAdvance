#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class NwmExtService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NWM_EXT;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, srvLogger)

	void controlWirelessEnabled(u32 messagePointer);
	void stubCommand(u32 messagePointer, const char* name);

  public:
	NwmExtService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
