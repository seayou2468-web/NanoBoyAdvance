#pragma once
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class SSLService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::SSL;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, sslLogger)

	std::mt19937 rng;  // Use a Mersenne Twister for RNG since this service is supposed to have better rng than just rand()
	bool initialized;
	u32 nextContextID = 1;
	u32 nextRootChainID = 1;
	u32 nextClientCertID = 1;

	struct Context {
		bool started = false;
		u32 rootChainID = 0;
		u32 clientCertID = 0;
		u32 options = 0;
		std::vector<u8> ioBuffer {};
	};

	std::unordered_map<u32, Context> contexts {};
	std::unordered_set<u32> rootChains {};
	std::unordered_set<u32> clientCerts {};

	// Service commands
	void initialize(u32 messagePointer);
	void generateRandomData(u32 messagePointer);
	void createContext(u32 messagePointer);
	void createRootCertChain(u32 messagePointer);
	void destroyRootCertChain(u32 messagePointer);
	void openClientCertContext(u32 messagePointer);
	void openDefaultClientCertContext(u32 messagePointer);
	void closeClientCertContext(u32 messagePointer);
	void initializeConnectionSession(u32 messagePointer);
	void startConnection(u32 messagePointer, bool withOut);
	void read(u32 messagePointer, bool peek);
	void write(u32 messagePointer);
	void contextSetRootCertChain(u32 messagePointer);
	void contextSetClientCert(u32 messagePointer);
	void contextClearOpt(u32 messagePointer);
	void contextGetProtocolCipher(u32 messagePointer);
	void destroyContext(u32 messagePointer);
	void contextInitSharedmem(u32 messagePointer);
	void acknowledgeChainOp(u32 messagePointer, u32 commandID);

  public:
	SSLService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
