#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>
#include <optional>
#include <string>
#include <unordered_map>

class BOSSService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::BOSS;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, bossLogger)

	// Service commands
	void cancelTask(u32 messagePointer);
	void deleteNsData(u32 messagePointer);
	void initializeSession(u32 messagePointer);
	void getAppNewFlag(u32 messagePointer);
	void getErrorCode(u32 messagePointer);
	void getNsDataHeaderInfo(u32 messagePointer);
	void getNewArrivalFlag(u32 messagePointer);
	void getNsDataIdList(u32 messagePointer, u32 commandWord);
	void getNsDataLastUpdated(u32 messagePointer);
	void getOptoutFlag(u32 messagePointer);
	void getStorageEntryInfo(u32 messagePointer);  // Unknown what this is, name taken from Citra
	void getTaskIdList(u32 messagePointer);
	void getTaskInfo(u32 messagePointer);
	void getTaskServiceStatus(u32 messagePointer);
	void getTaskState(u32 messagePointer);
	void getTaskStatus(u32 messagePointer);
	void getTaskStorageInfo(u32 messagePointer);
	void readNsData(u32 messagePointer);
	void receiveProperty(u32 messagePointer);
	void registerNewArrivalEvent(u32 messagePointer);
	void registerStorageEntry(u32 messagePointer);
	void registerTask(u32 messagePointer);
	void sendProperty(u32 messagePointer);
	void setAppNewFlag(u32 messagePointer);
	void setOptoutFlag(u32 messagePointer);
	void startBgImmediate(u32 messagePointer);
	void startTask(u32 messagePointer);
	void unregisterStorage(u32 messagePointer);
	void unregisterTask(u32 messagePointer);

	struct TaskInfo {
		std::string id;
		u8 state = 0;
		u8 status = 0;
	};

	std::unordered_map<std::string, TaskInfo> tasks;
	std::unordered_map<u32, std::array<u8, 256>> properties;
	std::unordered_map<u64, u8> appNewFlags;
	std::optional<Handle> newArrivalEvent = std::nullopt;
	u32 lastErrorCode = Result::Success;
	s8 optoutFlag;

  public:
	BOSSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
