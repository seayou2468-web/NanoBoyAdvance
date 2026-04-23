#include "../../../include/services/cecd.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"

namespace CECDCommands {
	enum : u32 {
		Initialize = 0x00010040,
		Deinitialize = 0x00020000,
		ResumeDaemon = 0x00030000,
		SuspendDaemon = 0x00040000,
		Write = 0x00050042,
		WriteMessage = 0x00060042,
		Delete = 0x00080000,
		SetData = 0x00090042,
		ReadData = 0x000A0042,
		Start = 0x000B0000,
		Stop = 0x000C0040,
		GetCecInfoBuffer = 0x000D0000,
		GetCecdState = 0x000E0000,
		GetInfoEventHandle = 0x000F0000,
		GetChangeStateEventHandle = 0x00100000,
		OpenAndWrite = 0x001100C2,
		OpenAndRead = 0x00120104,
	};
}

void CECDService::reset() {
	changeStateEvent = std::nullopt;
	infoEvent = std::nullopt;
}

void CECDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case CECDCommands::Initialize: {
			log("CECD::Initialize\n");
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case CECDCommands::GetInfoEventHandle: getInfoEventHandle(messagePointer); break;
		case CECDCommands::GetChangeStateEventHandle: getChangeStateEventHandle(messagePointer); break;
		case CECDCommands::OpenAndRead: openAndRead(messagePointer); break;
		case CECDCommands::Stop: stop(messagePointer); break;
		default:
			log("CECD unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void CECDService::getInfoEventHandle(u32 messagePointer) {
	log("CECD::GetInfoEventHandle\n");
	IPC::RequestParser rp(messagePointer, mem);
	if (!infoEvent.has_value()) infoEvent = kernel.makeEvent(ResetType::OneShot);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(*infoEvent);
}

void CECDService::getChangeStateEventHandle(u32 messagePointer) {
	log("CECD::GetChangeStateEventHandle\n");
	IPC::RequestParser rp(messagePointer, mem);
	if (!changeStateEvent.has_value()) changeStateEvent = kernel.makeEvent(ResetType::OneShot);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(*changeStateEvent);
}

void CECDService::openAndRead(u32 messagePointer) {
	log("CECD::OpenAndRead\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 2);
	rb.Push(Result::Success);
	rb.Push(0u); // Bytes read
}

void CECDService::stop(u32 messagePointer) {
	log("CECD::Stop\n");
	if (changeStateEvent.has_value()) kernel.signalEvent(*changeStateEvent);
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
