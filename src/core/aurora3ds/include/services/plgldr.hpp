#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class PLGLDRService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::PLG_LDR;
	Memory& mem;
	bool enabled = false;
	MAKE_LOG_FUNCTION(log, srvLogger)

  public:
	PLGLDRService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
