#pragma once
#include <unordered_map>
#include <unordered_set>

#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class HTTPService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::HTTP;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, httpLogger)

	bool initialized = false;
	u32 nextRootCertChainHandle = 1;
	std::unordered_map<u32, std::unordered_set<u32>> rootCertChains;

	// Service commands
	void createRootCertChain(u32 messagePointer);
	void initialize(u32 messagePointer);
	void rootCertChainAddDefaultCert(u32 messagePointer);

  public:
	HTTPService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
