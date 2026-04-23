#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class NIMService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NIM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nimLogger)

	// Service commands
	void initialize(u32 messagePointer);
	void commandStub(u32 messagePointer, u32 commandID);
	void commandStubWithU32(u32 messagePointer, u32 commandID, u32 value);

  public:
	NIMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
