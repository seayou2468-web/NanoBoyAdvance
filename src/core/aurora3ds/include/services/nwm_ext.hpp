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
	bool wirelessEnabled = true;
	u32 communicationMode = 0;
	u32 lastPolicy = 0;

	void controlWirelessEnabled(u32 messagePointer);
	void getWirelessEnabled(u32 messagePointer);
	void setCommunicationMode(u32 messagePointer);
	void getCommunicationMode(u32 messagePointer);
	void setPolicy(u32 messagePointer);
	void getPolicy(u32 messagePointer);
	void stubCommand(u32 messagePointer, const char* name);

  public:
	NwmExtService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
