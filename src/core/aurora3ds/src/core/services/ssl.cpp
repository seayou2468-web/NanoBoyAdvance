#include "../../../include/services/ssl.hpp"
#include "../../../include/ipc.hpp"
#include <algorithm>

namespace SSLCommands {
	enum : u32 {
		Initialize = 0x00010002,
		CreateContext = 0x00020082,
		GenerateRandomData = 0x00110042,
		DestroyContext = 0x001E0042,
	};
}

void SSLService::reset() {
	initialized = false;
	rng.seed(std::random_device{}());
}

void SSLService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	switch (command) {
		case SSLCommands::Initialize: initialize(messagePointer); break;
		case SSLCommands::CreateContext: createContext(messagePointer); break;
		case SSLCommands::GenerateRandomData: generateRandomData(messagePointer); break;
		case SSLCommands::DestroyContext: destroyContext(messagePointer); break;
		default:
			log("SSL service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void SSLService::initialize(u32 messagePointer) {
	log("SSL::Initialize\n");
	initialized = true;
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void SSLService::createContext(u32 messagePointer) {
	log("SSL::CreateContext\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(1u); // Dummy context handle
}

void SSLService::destroyContext(u32 messagePointer) {
	log("SSL::DestroyContext\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void SSLService::generateRandomData(u32 messagePointer) {
	IPC::RequestParser rp(messagePointer, mem);
	u32 size = rp.Pop();
	// Output pointer is in translate params
	log("SSL::GenerateRandomData (size = %08X)\n", size);

	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}
