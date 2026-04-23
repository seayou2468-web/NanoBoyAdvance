#pragma once
#include <random>
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class SSLService {
	using Handle = HorizonHandle;
	Handle handle = KernelHandles::SSL;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, sslLogger)
	std::mt19937 rng;
	bool initialized = false;

	void initialize(u32 messagePointer);
	void generateRandomData(u32 messagePointer);
	void createContext(u32 messagePointer);
	void destroyContext(u32 messagePointer);

  public:
	SSLService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
