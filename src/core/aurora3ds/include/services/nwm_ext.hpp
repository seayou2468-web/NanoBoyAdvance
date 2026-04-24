#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class NwmExtService {
	using Handle = HorizonHandle;

public:
	enum class Type { EXT, CEC, INF, SAP, SOC, TST };

private:
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
	void getMacAddress(u32 messagePointer);
	void genericSuccess(u32 messagePointer);
	void writePseudoMac(u32 outPointer, u32 outSize);
	void stubCommand(u32 messagePointer, const char* name);

public:
	NwmExtService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type = Type::EXT);
};
