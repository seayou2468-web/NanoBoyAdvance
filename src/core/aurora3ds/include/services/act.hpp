#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class ACTService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::ACT;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, actLogger)
	u32 nextUuidCounter = 1;
	u32 lastResultCode = ResultCode::Success;

	// Service commands
	void initialize(u32 messagePointer);
	void getErrorCode(u32 messagePointer);
	void generateUUID(u32 messagePointer);
	void getAccountDataBlock(u32 messagePointer);
	void respondNotImplemented(u32 messagePointer, u32 command);

  public:
	ACTService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
