#include "../../../include/services/mvd_std.hpp"
#include <algorithm>
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

void MVDStdService::reset() {
	initialized = false;
	status = 0;
	frameCounter = 0;
	currentWidth = 0;
	currentHeight = 0;
	outputLuma = 0;
	outputChroma = 0;
}

void MVDStdService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command & 0xFFFF0000) {
		case MVDCommands::Initialize: {
			log("mvd:std::Initialize\n");
			initialized = true;
			status = 1;
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
		}
		case MVDCommands::GetStatus: {
			auto rb = rp.MakeBuilder(3, 0);
			rb.Push(Result::Success);
			rb.Push(status);
			rb.Push(frameCounter);
			break;
		}
		default:
			log("mvd:std unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void MVDStdService::initialize(u32 messagePointer) {}
void MVDStdService::shutdown(u32 messagePointer) {}
void MVDStdService::calculateWorkBufSize(u32 messagePointer) {}
void MVDStdService::calculateImageSize(u32 messagePointer) {}
void MVDStdService::processNALUnit(u32 messagePointer) {}
void MVDStdService::controlFrameRendering(u32 messagePointer) {}
void MVDStdService::getStatus(u32 messagePointer, u32 id) {}
void MVDStdService::getConfig(u32 messagePointer) {}
void MVDStdService::setConfig(u32 messagePointer) {}
void MVDStdService::setOutputBuffer(u32 messagePointer) {}
void MVDStdService::overrideOutputBuffers(u32 messagePointer) {}
void MVDStdService::stubCommand(u32 messagePointer, const char* name) {}
