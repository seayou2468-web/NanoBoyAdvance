#pragma once
#include "../helpers.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class ErrFService {
	Memory& mem;
	MAKE_LOG_FUNCTION(log, errLogger)

  public:
	ErrFService(Memory& mem) : mem(mem) {}
	void reset() {}
	void handleSyncRequest(u32 messagePointer);
  private:
	void throwFatalError(u32 messagePointer);
};
