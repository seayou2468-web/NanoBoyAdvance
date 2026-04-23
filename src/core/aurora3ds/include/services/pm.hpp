#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class PMService {
	using Handle = HorizonHandle;

	Handle appHandle = KernelHandles::PM_APP;
	Handle dbgHandle = KernelHandles::PM_DBG;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, srvLogger)

  public:
	enum class Type : u8 {
		App,
		Debug,
	};

	PMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};
