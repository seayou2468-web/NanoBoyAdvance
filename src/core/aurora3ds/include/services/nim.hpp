#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>
#include <unordered_set>
#include <vector>

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
	u32 asyncResult = Result::Success;
	bool asyncPending = false;
	std::vector<u8> tslXml {};
	std::vector<u8> dtlXml {};
	std::vector<u8> autoDbgDat {};
	std::vector<u8> dbgTasks {};
	std::array<u8, 0x24> savedHash {};
	std::unordered_set<u64> registeredTitleTasks {};
	u32 customerSupportCode = 0;
	bool systemTitlesCommittable = false;

	// Service commands
	void initialize(u32 messagePointer);
	void startNetworkUpdate(u32 messagePointer);
	void startDownload(u32 messagePointer);
	void isSystemUpdateAvailable(u32 messagePointer);
	void getNumAutoTitleDownloadTasks(u32 messagePointer);
	void getAutoTitleDownloadTaskInfos(u32 messagePointer);
	void getTslXmlSize(u32 messagePointer);
	void getDtlXmlSize(u32 messagePointer);
	void getProgress(u32 messagePointer);
	void getTitleDownloadProgress(u32 messagePointer);
	void writeRawBuffer(u32 src, u32 size, std::vector<u8>& outBuffer);
	void readRawBuffer(u32 dst, const std::vector<u8>& inBuffer);

  public:
	NIMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
