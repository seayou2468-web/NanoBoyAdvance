#include "../../../include/services/act.hpp"
#include "../../../include/ipc.hpp"

namespace ACTCommands {
	enum : u32 {
		Initialize = 0x00010084,
		GetCommonInfo = 0x00050042,
		GetAccountInfo = 0x000600C2,
		GenerateUUID = 0x000D0040,
	};
}

void ACTService::reset() {}

void ACTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case ACTCommands::Initialize: initialize(messagePointer); break;
		case ACTCommands::GetCommonInfo: getCommonInfo(messagePointer); break;
		case ACTCommands::GetAccountInfo: getAccountDataBlock(messagePointer); break; // Reference uses GetAccountInfo for 0x6
		case ACTCommands::GenerateUUID: generateUUID(messagePointer); break;
		default:
			log("ACT service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void ACTService::initialize(u32 messagePointer) {
	log("ACT::Initialize\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void ACTService::getCommonInfo(u32 messagePointer) {
	log("ACT::GetCommonInfo\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}

void ACTService::generateUUID(u32 messagePointer) {
	log("ACT::GenerateUUID\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(5, 0);
	rb.Push(Result::Success);
	rb.Push(0u); rb.Push(0u); rb.Push(0u); rb.Push(0u); // UUID
}

void ACTService::getAccountDataBlock(u32 messagePointer) {
	log("ACT::GetAccountDataBlock\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 2);
	rb.Push(Result::Success);
}
