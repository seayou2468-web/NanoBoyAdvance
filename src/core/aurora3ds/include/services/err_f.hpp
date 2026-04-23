#pragma once
#include "../helpers.hpp"
#include "../kernel/handles.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class ErrFService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::ERR_F;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, errLogger)

	void throwFatalError(u32 messagePointer);

  public:
	explicit ErrFService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
