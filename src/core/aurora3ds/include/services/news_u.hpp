#pragma once
#include "../helpers.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class NewsUService {
	Memory& mem;
	MAKE_LOG_FUNCTION(log, newsLogger)

  public:
	NewsUService(Memory& mem) : mem(mem) {}
	void reset() {}
	void handleSyncRequest(u32 messagePointer);
  private:
	void addNotification(u32 messagePointer);
};
