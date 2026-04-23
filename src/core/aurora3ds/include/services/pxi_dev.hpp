#pragma once
#include "../helpers.hpp"
#include "../kernel/handles.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class PXIDevService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::PXI_DEV;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, pxiLogger)

  public:
	explicit PXIDevService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
