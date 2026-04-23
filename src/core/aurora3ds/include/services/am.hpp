#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

#include <map>
#include <unordered_set>
#include <vector>

class AMService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::AM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, amLogger)

	// Service commands
	void respondNotImplemented(u32 messagePointer, u32 command);
	void getDLCTitleInfo(u32 messagePointer);
	void getPatchTitleInfo(u32 messagePointer);
	void listTitleInfo(u32 messagePointer);

	u32 nextImportProgramHandle = 1;
	u32 nextImportTicketHandle = 1;
	std::unordered_set<u32> importProgramHandles;
	std::unordered_set<u32> importTicketHandles;
	std::vector<u64> installedPrograms;
	std::multimap<u64, u64> ticketMap;

  public:
	AMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
