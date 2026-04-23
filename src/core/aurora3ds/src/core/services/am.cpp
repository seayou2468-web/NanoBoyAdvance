#include "../../../include/services/am.hpp"
#include "../../../include/ipc.hpp"
#include <algorithm>

namespace AMCommands {
	enum : u32 {
		GetNumPrograms = 0x00010040,
		GetProgramList = 0x00020082,
		GetProgramInfos = 0x00030082,
		GetDLCTitleInfo = 0x10050084,
		ListTitleInfo = 0x10070102,
		GetPatchTitleInfo = 0x100D0084,
		GetDeviceID = 0x000A0000,
	};
}

void AMService::reset() {}

void AMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case AMCommands::GetNumPrograms: getNumPrograms(messagePointer); break;
		case AMCommands::GetProgramList: getProgramList(messagePointer); break;
		case AMCommands::GetProgramInfos: getProgramInfos(messagePointer); break;
		case AMCommands::GetPatchTitleInfo: getPatchTitleInfo(messagePointer); break;
		case AMCommands::GetDLCTitleInfo: getDLCTitleInfo(messagePointer); break;
		case AMCommands::ListTitleInfo: listTitleInfo(messagePointer); break;
		case AMCommands::GetDeviceID: getDeviceID(messagePointer); break;
		default:
			log("AM service requested unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void AMService::getNumPrograms(u32 messagePointer) {
	log("AM::GetNumPrograms\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // 0 programs for now
}

void AMService::getProgramList(u32 messagePointer) {
	log("AM::GetProgramList\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0u); // 0 programs written
}

void AMService::getProgramInfos(u32 messagePointer) {
	log("AM::GetProgramInfos\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void AMService::getDeviceID(u32 messagePointer) {
	log("AM::GetDeviceID\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0x12345678u); // Dummy device ID
}

void AMService::listTitleInfo(u32 messagePointer) {
	log("AM::ListTitleInfo\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0u);
}

void AMService::getDLCTitleInfo(u32 messagePointer) {
	log("AM::GetDLCTitleInfo\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void AMService::getPatchTitleInfo(u32 messagePointer) {
	log("AM::GetPatchTitleInfo\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}
