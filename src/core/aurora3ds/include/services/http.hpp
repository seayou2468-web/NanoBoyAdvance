#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

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
	u32 nextContextHandle = 1;
	std::unordered_map<u32, std::unordered_set<u32>> rootCertChains;
	struct HTTPContext {
		u32 method = 0;
		std::string url;
		u32 statusCode = 200;
		std::vector<u8> responseData;
		u32 readOffset = 0;
		bool requestStarted = false;
	};
	std::unordered_map<u32, HTTPContext> contexts;

	// Service commands
	void createContext(u32 messagePointer);
	void closeContext(u32 messagePointer);
	void beginRequest(u32 messagePointer);
	void getResponseStatusCode(u32 messagePointer);
	void receiveData(u32 messagePointer);
	void createRootCertChain(u32 messagePointer);
	void initialize(u32 messagePointer);
	void rootCertChainAddDefaultCert(u32 messagePointer);

  public:
	HTTPService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
