#include "../../../include/services/apt.hpp"
#include <algorithm>
#include <vector>
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040,
		Initialize = 0x00020080,
		Enable = 0x00030040,
		GetAppletInfo = 0x00060040,
		IsRegistered = 0x00090040,
		InquireNotification = 0x000B0040,
		SendParameter = 0x000C0104,
		ReceiveParameter = 0x000D0080,
		GlanceParameter = 0x000E0080,
		PreloadLibraryApplet = 0x00160040,
		PrepareToStartLibraryApplet = 0x00180040,
		StartLibraryApplet = 0x001E0084,
		ReplySleepQuery = 0x003E0080,
		NotifyToWait = 0x00430040,
		GetSharedFont = 0x00440000,
		GetWirelessRebootInfo = 0x00450040,
		AppletUtility = 0x004B00C2,
		SetApplicationCpuTimeLimit = 0x004F0080,
		GetApplicationCpuTimeLimit = 0x00500040,
		GetProgramId = 0x00580040,
		SetScreencapPostPermission = 0x00550040,
		GetTargetPlatform = 0x01010000,
		CheckNew3DSApp = 0x01010000, // Same ID? Reference says 0x0101 is GetTargetPlatform
		CheckNew3DS = 0x01020000,
		TheSmashBrosFunction = 0x01030000
	};
}

void APTService::reset() {
	cpuTimeLimit = 0;
	lockHandle = std::nullopt;
	notificationEvent = std::nullopt;
	parameterEvent = std::nullopt;
	resumeEvent = std::nullopt;
	wirelessRebootInfo.clear();
	appletManager.reset();
}

void APTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case APTCommands::AppletUtility: appletUtility(messagePointer); break;
		case APTCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case APTCommands::CheckNew3DSApp: checkNew3DSApp(messagePointer); break;
		case APTCommands::Enable: enable(messagePointer); break;
		case APTCommands::GetAppletInfo: getAppletInfo(messagePointer); break;
		case APTCommands::GetSharedFont: getSharedFont(messagePointer); break;
		case APTCommands::Initialize: initialize(messagePointer); break;
		case APTCommands::InquireNotification: inquireNotification(messagePointer); break;
		case APTCommands::IsRegistered: isRegistered(messagePointer); break;
		case APTCommands::GetApplicationCpuTimeLimit: getApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		case APTCommands::GetWirelessRebootInfo: getWirelessRebootInfo(messagePointer); break;
		case APTCommands::GlanceParameter: glanceParameter(messagePointer); break;
		case APTCommands::NotifyToWait: notifyToWait(messagePointer); break;
		case APTCommands::PreloadLibraryApplet: preloadLibraryApplet(messagePointer); break;
		case APTCommands::PrepareToStartLibraryApplet: prepareToStartLibraryApplet(messagePointer); break;
		case APTCommands::StartLibraryApplet: startLibraryApplet(messagePointer); break;
		case APTCommands::ReceiveParameter: receiveParameter(messagePointer); break;
		case APTCommands::ReplySleepQuery: replySleepQuery(messagePointer); break;
		case APTCommands::SetApplicationCpuTimeLimit: setApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::SendParameter: sendParameter(messagePointer); break;
		case APTCommands::SetScreencapPostPermission: setScreencapPostPermission(messagePointer); break;
		case APTCommands::TheSmashBrosFunction: theSmashBrosFunction(messagePointer); break;
		case APTCommands::GetProgramId: getProgramId(messagePointer); break;
		default:
			log("APT service requested unknown command: %08X\n", command);
			mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void APTService::initialize(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 appId = rp.Pop();
	u32 attributes = rp.Pop();
	log("APT::Initialize(appId = %x, attributes = %x)\n", appId, attributes);

	if (!notificationEvent) notificationEvent = kernel.makeEvent(ResetType::Sticky);
	if (!parameterEvent) parameterEvent = kernel.makeEvent(ResetType::Sticky);

	auto rb = rp.MakeBuilder(1, 4);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(2));
	rb.Push(notificationEvent.value());
	rb.Push(parameterEvent.value());
}

void APTService::getLockHandle(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 flags = rp.Pop();
	log("APT::GetLockHandle(flags = %x)\n", flags);

	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(KernelHandles::APT); // Placeholder
}

void APTService::enable(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 unknown = rp.Pop();
	log("APT::Enable(unknown = %x)\n", unknown);

	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::getAppletInfo(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 appId = rp.Pop();
	log("APT::GetAppletInfo(appId = %x)\n", appId);

	auto rb = rp.MakeBuilder(7, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // Title ID low
	rb.Push(0u); // Title ID high
	rb.Push(0u); // Media type
	rb.Push(1u); // Registered
	rb.Push(1u); // Loaded
	rb.Push(0u); // Attributes
}

void APTService::isRegistered(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 appId = rp.Pop();
	log("APT::IsRegistered(appId = %x)\n", appId);

	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u); // Always registered for now
}

void APTService::inquireNotification(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 appId = rp.Pop();
	log("APT::InquireNotification(appId = %x)\n", appId);

	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // None
}

void APTService::sendParameter(u32 messagePointer) {
	log("APT::SendParameter\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::receiveParameter(u32 messagePointer) {
	log("APT::ReceiveParameter\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(7, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // Sender
	rb.Push(0u); // Destination
	rb.Push(1u); // Signal (Wakeup)
	rb.Push(0u); // Buffer size
	rb.Push(0u); // Unknown
	rb.Push(0u); // Unknown
}

void APTService::glanceParameter(u32 messagePointer) {
	log("APT::GlanceParameter\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(7, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // Sender
	rb.Push(0u); // Destination
	rb.Push(1u); // Signal (Wakeup)
	rb.Push(0u); // Buffer size
	rb.Push(0u); // Unknown
	rb.Push(0u); // Unknown
}

void APTService::appletUtility(u32 messagePointer) {
	log("APT::AppletUtility\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 2);
	rb.Push(Result::Success);
	rb.Push(Result::Success);
}

void APTService::getSharedFont(u32 messagePointer) {
	log("APT::GetSharedFont\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 2);
	rb.Push(Result::Success);
	rb.Push(0u); // Font size
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(KernelHandles::FontSharedMemHandle);
}

void APTService::getWirelessRebootInfo(u32 messagePointer) {
	log("APT::GetWirelessRebootInfo\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}

void APTService::replySleepQuery(u32 messagePointer) {
	log("APT::ReplySleepQuery\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::notifyToWait(u32 messagePointer) {
	log("APT::NotifyToWait\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::getProgramId(u32 messagePointer) {
	log("APT::GetProgramId\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(3, 0);
	rb.Push(Result::Success);
	rb.Push(0ULL); // TODO: Get actual program ID
}

void APTService::checkNew3DS(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u); // Hardcoded New3DS for now
}

void APTService::checkNew3DSApp(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u);
}

void APTService::getApplicationCpuTimeLimit(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(cpuTimeLimit);
}

void APTService::setApplicationCpuTimeLimit(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	cpuTimeLimit = rp.Pop();
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::preloadLibraryApplet(u32 messagePointer) {
	log("APT::PreloadLibraryApplet\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::prepareToStartLibraryApplet(u32 messagePointer) {
	log("APT::PrepareToStartLibraryApplet\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::startLibraryApplet(u32 messagePointer) {
	log("APT::StartLibraryApplet\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::setScreencapPostPermission(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	screencapPostPermission = rp.Pop();
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void APTService::theSmashBrosFunction(u32 messagePointer) {
	log("APT::TheSmashBrosFunction\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
