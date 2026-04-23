#include "../../../../include/services/ir/ir_user.hpp"
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>
#include "../../../../include/ipc.hpp"
#include "../../../../include/kernel/kernel.hpp"
#include "../../../../include/services/ir/ir_types.hpp"

using namespace IR;

namespace IRUserCommands {
	enum : u32 {
		Initialize = 0x00010182,
		Shutdown = 0x00020000,
		StartSendTransfer = 0x00030042,
		WaitSendTransfer = 0x00040000,
		StartRecvTransfer = 0x00050042,
		WaitRecvTransfer = 0x00060000,
		GetRecvTransferCount = 0x00070000,
		GetSendState = 0x00080000,
		SetBitRate = 0x00090040,
		GetBitRate = 0x000A0000,
		SetIRLEDState = 0x000B0040,
		GetIRLEDRecvState = 0x000C0000,
		GetSendFinishedEvent = 0x000D0000,
		GetRecvFinishedEvent = 0x000E0000,
		GetTransferState = 0x000F0000,
		GetErrorStatus = 0x00100000,
		SetSleepModeActive = 0x00110040,
		SetSleepModeState = 0x00120040,
		GetConnectionStatus = 0x00130000,
		GetTryingToConnectStatus = 0x00140000,
		GetReceiveSizeFreeAndUsed = 0x00150000,
		GetSendSizeFreeAndUsed = 0x00160000,
		GetConnectionRole = 0x00170000,
		InitializeIrNopShared = 0x00180182,
		ReleaseReceivedData = 0x00190040,
		SetOwnMachineId = 0x001A0040,
	};
}

IRUserService::IRUserService(Memory& mem, HIDService& hid, const EmulatorConfig& config, Kernel& kernel)
	: mem(mem), hid(hid), config(config), kernel(kernel), cpp([&](IR::Device::Payload payload) { sendPayload(payload); }, kernel.getScheduler()) {}

void IRUserService::reset() {
	connectionStatusEvent = std::nullopt;
	receiveEvent = std::nullopt;
	sendEvent = std::nullopt;
	sharedMemory = std::nullopt;
	receiveBuffer = nullptr;
	connectedDevice = false;
}

void IRUserService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	switch (command) {
		case IRUserCommands::Initialize: {
			log("IR:USER: Initialize\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case IRUserCommands::Shutdown: {
			log("IR:USER: Shutdown\n");
			connectedDevice = false;
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case IRUserCommands::GetReceiveEvent: getReceiveEvent(messagePointer); break;
		case IRUserCommands::GetSendEvent: getSendEvent(messagePointer); break;
		case IRUserCommands::GetConnectionStatusEvent: getConnectionStatusEvent(messagePointer); break;
		case IRUserCommands::InitializeIrnopShared: initializeIrnopShared(messagePointer); break;
		case IRUserCommands::ReleaseReceivedData: releaseReceivedData(messagePointer); break;
		case IRUserCommands::GetConnectionStatus: {
			log("IR:USER: GetConnectionStatus\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(connectedDevice ? 2u : 0u);
			break;
		}
		default:
			log("IR:USER unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void IRUserService::initializeIrnopShared(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	const u32 sharedMemSize = rp.Pop();
	const u32 receiveBufferSize = rp.Pop();
	const u32 receiveBufferPackageCount = rp.Pop();
	const u32 sendBufferSize = rp.Pop();
	const u32 sendBufferPackageCount = rp.Pop();
	const u32 bitrate = rp.Pop();
	// Descriptor and handle are next

	log("IR:USER: InitializeIrnopShared (shared mem size = %08X)\n", sharedMemSize);

	if (!receiveEvent) receiveEvent = kernel.makeEvent(ResetType::OneShot);
	if (!sendEvent) sendEvent = kernel.makeEvent(ResetType::OneShot);
	if (!connectionStatusEvent) connectionStatusEvent = kernel.makeEvent(ResetType::OneShot);

	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void IRUserService::getReceiveEvent(u32 messagePointer) {
	log("IR:USER: GetReceiveEvent\n");
	IPC::RequestParser rp(messagePointer, mem);
	if (!receiveEvent) receiveEvent = kernel.makeEvent(ResetType::OneShot);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(receiveEvent.value());
}

void IRUserService::getSendEvent(u32 messagePointer) {
	log("IR:USER: GetSendEvent\n");
	IPC::RequestParser rp(messagePointer, mem);
	if (!sendEvent) sendEvent = kernel.makeEvent(ResetType::OneShot);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(sendEvent.value());
}

void IRUserService::getConnectionStatusEvent(u32 messagePointer) {
	log("IR:USER: GetConnectionStatusEvent\n");
	IPC::RequestParser rp(messagePointer, mem);
	if (!connectionStatusEvent) connectionStatusEvent = kernel.makeEvent(ResetType::OneShot);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(connectionStatusEvent.value());
}

void IRUserService::releaseReceivedData(u32 messagePointer) {
	log("IR:USER: ReleaseReceivedData\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void IRUserService::sendPayload(Payload payload) {
	// TODO: Write payload to shared memory
}

void IRUserService::updateCirclePadPro() {
	// TODO: Handle CPP updates
}
