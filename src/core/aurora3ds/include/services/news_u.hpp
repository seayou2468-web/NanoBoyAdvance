#pragma once
#include <array>

#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class NewsUService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NEWS_U;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, newsLogger)

	static constexpr u32 MaxNotifications = 100;

	u32 totalNotifications = 0;
	bool automaticSyncFlag = false;
	std::array<u32, MaxNotifications> messageSizes{};
	std::array<u32, MaxNotifications> imageSizes{};

	// Service commands
	void addNotification(u32 messagePointer);
	void addNotificationSystem(u32 messagePointer);
	void resetNotifications(u32 messagePointer);
	void getTotalNotifications(u32 messagePointer);
	void setNewsDBHeader(u32 messagePointer);
	void setNotificationHeader(u32 messagePointer);
	void setNotificationMessage(u32 messagePointer);
	void setNotificationImage(u32 messagePointer);
	void getNewsDBHeader(u32 messagePointer);
	void getNotificationHeader(u32 messagePointer);
	void getNotificationMessage(u32 messagePointer);
	void getNotificationImage(u32 messagePointer);
	void setAutomaticSyncFlag(u32 messagePointer);
	void setNotificationHeaderOther(u32 messagePointer);
	void writeNewsDBSavedata(u32 messagePointer);
	void getTotalArrivedNotifications(u32 messagePointer);

  public:
	NewsUService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
