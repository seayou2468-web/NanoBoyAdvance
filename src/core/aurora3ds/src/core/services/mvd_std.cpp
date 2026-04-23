#include "../../../include/services/mvd_std.hpp"

#include "../../../include/ipc.hpp"

namespace MVDCommands {
	enum : u32 {
		Initialize = 0x00010000,
		Shutdown = 0x00020000,
		CalculateWorkBufSize = 0x00030000,
		CalculateImageSize = 0x00040000,
		ProcessNALUnit = 0x00080000,
		ControlFrameRendering = 0x00090000,
		GetStatus = 0x000A0000,
		GetStatusOther = 0x000B0000,
		GetConfig = 0x001D0000,
		SetConfig = 0x001E0000,
		SetOutputBuffer = 0x001F0000,
		OverrideOutputBuffers = 0x00210000,
	};
}

void MVDStdService::reset() {}

void MVDStdService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command & 0xFFFF0000) {
		case MVDCommands::Initialize: initialize(messagePointer); break;
		case MVDCommands::Shutdown: shutdown(messagePointer); break;
		case MVDCommands::CalculateWorkBufSize: calculateWorkBufSize(messagePointer); break;
		case MVDCommands::CalculateImageSize: calculateImageSize(messagePointer); break;
		case MVDCommands::ProcessNALUnit: stubCommand(messagePointer, "ProcessNALUnit"); break;
		case MVDCommands::ControlFrameRendering: stubCommand(messagePointer, "ControlFrameRendering"); break;
		case MVDCommands::GetStatus: stubCommand(messagePointer, "GetStatus"); break;
		case MVDCommands::GetStatusOther: stubCommand(messagePointer, "GetStatusOther"); break;
		case MVDCommands::GetConfig: stubCommand(messagePointer, "GetConfig"); break;
		case MVDCommands::SetConfig: stubCommand(messagePointer, "SetConfig"); break;
		case MVDCommands::SetOutputBuffer: stubCommand(messagePointer, "SetOutputBuffer"); break;
		case MVDCommands::OverrideOutputBuffers: stubCommand(messagePointer, "OverrideOutputBuffers"); break;
		default: Helpers::panic("mvd:std service requested. Command: %08X\n", command);
	}
}

void MVDStdService::initialize(u32 messagePointer) {
	log("mvd:std::Initialize [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::shutdown(u32 messagePointer) {
	log("mvd:std::Shutdown [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::calculateWorkBufSize(u32 messagePointer) {
	log("mvd:std::CalculateWorkBufSize [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x1000);
}

void MVDStdService::calculateImageSize(u32 messagePointer) {
	log("mvd:std::CalculateImageSize [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}

void MVDStdService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("mvd:std::%s (command=%08X) [stubbed]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
