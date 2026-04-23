#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class NIMService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NIM;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nimLogger)
	bool initialized = false;
	bool networkUpdateActive = false;
	bool titleDownloadActive = false;
	u32 lastDownloadTitleIDLow = 0;
	u32 lastDownloadTitleIDHigh = 0;
	u32 systemUpdateProgress = 0;
	u32 titleDownloadProgress = 0;

	// Service commands
	void initialize(u32 messagePointer);
	void startNetworkUpdate(u32 messagePointer);
	void startDownload(u32 messagePointer);
	void isSystemUpdateAvailable(u32 messagePointer);
	void getNumAutoTitleDownloadTasks(u32 messagePointer);
	void getAutoTitleDownloadTaskInfos(u32 messagePointer);
	void getTslXmlSize(u32 messagePointer);
	void getDtlXmlSize(u32 messagePointer);
	void commandStub(u32 messagePointer, u32 commandID);
	void commandStubWithU32(u32 messagePointer, u32 commandID, u32 value);
	void commandStubWithHandle(u32 messagePointer, u32 commandID, Handle handle);
	void getProgress(u32 messagePointer);
	void getTitleDownloadProgress(u32 messagePointer);

  public:
	NIMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
