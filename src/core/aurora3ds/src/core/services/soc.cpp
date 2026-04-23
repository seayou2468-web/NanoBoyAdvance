#include "../../../include/services/soc.hpp"
#include "../../../include/ipc.hpp"
#include "../../../include/result/result.hpp"

namespace SOCCommands {
	enum : u32 {
		InitializeSockets = 0x00010044,
		Socket = 0x000200C2,
		Listen = 0x00030082,
		Accept = 0x00040082,
		Bind = 0x00050082,
		Connect = 0x00060082,
		RecvFrom = 0x00080102,
		SendToSingle = 0x000A0102,
		Close = 0x000B0042,
		GetAddrInfo = 0x000F0102,
	};
}

void SOCService::reset() { initialized = false; }

void SOCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	switch (command) {
		case SOCCommands::InitializeSockets: initializeSockets(messagePointer); break;
		case SOCCommands::Socket: {
			log("SOC::Socket\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(1u); // Socket FD
			break;
		}
		case SOCCommands::Close: {
			log("SOC::Close\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(0u);
			break;
		}
		default:
			log("SOC service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void SOCService::initializeSockets(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 memoryBlockSize = rp.Pop();
	// Descriptor and handle are in translate params
	log("SOC::InitializeSockets (memory block size = %08X)\n", memoryBlockSize);

	initialized = true;

	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
