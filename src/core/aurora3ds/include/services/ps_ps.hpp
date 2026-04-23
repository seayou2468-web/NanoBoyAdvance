#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include <random>

class PSPSService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::PS_PS;
	Memory& mem;
	std::mt19937 rng;
	MAKE_LOG_FUNCTION(log, srvLogger)

	void generateRandomBytes(u32 messagePointer);
	void seedRNG(u32 messagePointer);
	void stubCommand(u32 messagePointer, const char* name);

  public:
	PSPSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
