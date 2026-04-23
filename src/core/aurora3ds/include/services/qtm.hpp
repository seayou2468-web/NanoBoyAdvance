#pragma once
#include "../helpers.hpp"
#include "../kernel/handles.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class QTMService {
  public:
	enum class Type {
		C,
		S,
		SP,
		U
	};

  private:
	Memory& mem;
	MAKE_LOG_FUNCTION(log, qtmLogger)

	void initializeHardwareCheck(u32 messagePointer);
	void setIrLedCheck(u32 messagePointer);
	void getHeadtrackingInfoRaw(u32 messagePointer, Type type);
	void getHeadtrackingInfo(u32 messagePointer, Type type);

  public:
	explicit QTMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};
