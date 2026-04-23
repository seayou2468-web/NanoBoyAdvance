#pragma once
#include <optional>
#include <vector>

#include "../applets/applet_manager.hpp"
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

// Yay, more circular dependencies
class Kernel;

enum class ConsoleModel : u32 {
	Old3DS,
	New3DS,
};

namespace APT {
	enum class AppletId : u32 {
		None = 0x0,
		Any = 0x1,
		Application = 0x300,
		HomeMenu = 0x101,
		AlternateMenu = 0x103,
		MiiSelector = 0x110,
		SoftwareKeyboard = 0x111,
		ErrDisp = 0x112,
		FriendsList = 0x113,
		SettingsApplet = 0x114,
		WebBrowser = 0x115,
		Miiverse = 0x116,
		NintendoZone = 0x117,
		PhotoViewer = 0x118,
		SystemSettings = 0x119,
		AppletUtility = 0x11A,
		EShop = 0x11B,
		TrainingLabel = 0x11C,
		Passport = 0x11D,
		Ar = 0x11E,
		FaceRaiders = 0x11F,
		MiiMaker = 0x120,
		SystemUpdate = 0x121,
		NintendoStore = 0x122,
		InstructionManual = 0x123,
		DreamLog = 0x124,
	};
}

class APTService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::APT;
	Memory& mem;
	Kernel& kernel;

	std::optional<Handle> lockHandle = std::nullopt;
	std::optional<Handle> notificationEvent = std::nullopt;
	std::optional<Handle> parameterEvent = std::nullopt;
	std::optional<Handle> resumeEvent = std::nullopt;

	ConsoleModel model = ConsoleModel::Old3DS;
	Applets::AppletManager appletManager;

	MAKE_LOG_FUNCTION(log, aptLogger)

	// Service commands
	void appletUtility(u32 messagePointer);
	void getApplicationCpuTimeLimit(u32 messagePointer);
	void getLockHandle(u32 messagePointer);
	void checkNew3DS(u32 messagePointer);
	void checkNew3DSApp(u32 messagePointer);
	void enable(u32 messagePointer);
	void getAppletInfo(u32 messagePointer);
	void getSharedFont(u32 messagePointer);
	void getWirelessRebootInfo(u32 messagePointer);
	void glanceParameter(u32 messagePointer);
	void initialize(u32 messagePointer);
	void inquireNotification(u32 messagePointer);
	void isRegistered(u32 messagePointer);
	void notifyToWait(u32 messagePointer);
	void preloadLibraryApplet(u32 messagePointer);
	void prepareToStartLibraryApplet(u32 messagePointer);
	void receiveParameter(u32 messagePointer);
	void replySleepQuery(u32 messagePointer);
	void setApplicationCpuTimeLimit(u32 messagePointer);
	void setScreencapPostPermission(u32 messagePointer);
	void sendParameter(u32 messagePointer);
	void startLibraryApplet(u32 messagePointer);
	void theSmashBrosFunction(u32 messagePointer);
	void getProgramId(u32 messagePointer);
	void getTargetPlatform(u32 messagePointer);

	// Percentage of the syscore available to the application, between 5% and 89%
	u32 cpuTimeLimit = 0;
	u32 screencapPostPermission = 0;
	std::vector<u8> wirelessRebootInfo;

  public:
	APTService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel), appletManager(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
