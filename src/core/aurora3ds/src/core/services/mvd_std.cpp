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

	switch (command & 0xFFFF0000) {
		case MVDCommands::Initialize: initialize(messagePointer); break;
		case MVDCommands::Shutdown: shutdown(messagePointer); break;
		case MVDCommands::CalculateWorkBufSize: calculateWorkBufSize(messagePointer); break;
		case MVDCommands::CalculateImageSize: calculateImageSize(messagePointer); break;
		case MVDCommands::ProcessNALUnit: processNALUnit(messagePointer); break;
		case MVDCommands::ControlFrameRendering: controlFrameRendering(messagePointer); break;
		case MVDCommands::GetStatus: getStatus(messagePointer, 0xA); break;
		case MVDCommands::GetStatusOther: getStatus(messagePointer, 0xB); break;
		case MVDCommands::GetConfig: getConfig(messagePointer); break;
		case MVDCommands::SetConfig: setConfig(messagePointer); break;
		case MVDCommands::SetOutputBuffer: setOutputBuffer(messagePointer); break;
		case MVDCommands::OverrideOutputBuffers: overrideOutputBuffers(messagePointer); break;
		default: Helpers::panic("mvd:std service requested. Command: %08X\n", command);
	}
}

void MVDStdService::initialize(u32 messagePointer) {
	log("mvd:std::Initialize\n");
	initialized = true;
	status = 1;
	frameCounter = 0;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::shutdown(u32 messagePointer) {
	log("mvd:std::Shutdown\n");
	initialized = false;
	status = 0;

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::calculateWorkBufSize(u32 messagePointer) {
	const u32 width = mem.read32(messagePointer + 4);
	const u32 height = mem.read32(messagePointer + 8);
	log("mvd:std::CalculateWorkBufSize (width=%u, height=%u)\n", width, height);

	// Lightweight compatibility estimate, aligned to 4KB.
	u32 estimated = width * height;
	estimated = std::max<u32>(estimated, 0x1000);
	estimated = (estimated + 0xFFF) & ~0xFFF;

	mem.write32(messagePointer, IPC::responseHeader(0x3, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, estimated);
}

void MVDStdService::calculateImageSize(u32 messagePointer) {
	const u32 width = mem.read32(messagePointer + 4);
	const u32 height = mem.read32(messagePointer + 8);
	log("mvd:std::CalculateImageSize (width=%u, height=%u)\n", width, height);

	// YUV420 output surface estimate.
	const u32 imageSize = (width * height * 3) / 2;

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, imageSize);
}

void MVDStdService::processNALUnit(u32 messagePointer) {
	const u32 nalSize = mem.read32(messagePointer + 4);
	log("mvd:std::ProcessNALUnit (size=%u)\n", nalSize);

	if (initialized) {
		frameCounter++;
		status = 2;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::controlFrameRendering(u32 messagePointer) {
	const u32 enable = mem.read32(messagePointer + 4);
	log("mvd:std::ControlFrameRendering (enable=%u)\n", enable);

	if (initialized) {
		status = (enable != 0) ? 2 : 1;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::getStatus(u32 messagePointer, u32 commandID) {
	log("mvd:std::GetStatus (status=%u, frames=%u)\n", status, frameCounter);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, status);
	mem.write32(messagePointer + 12, frameCounter);
}

void MVDStdService::getConfig(u32 messagePointer) {
	const u32 id = mem.read32(messagePointer + 4);
	u32 value = 0;

	switch (id) {
		case 0: value = currentWidth; break;
		case 1: value = currentHeight; break;
		case 2: value = outputLuma; break;
		case 3: value = outputChroma; break;
		default: break;
	}

	log("mvd:std::GetConfig (id=%u, value=%u)\n", id, value);
	mem.write32(messagePointer, IPC::responseHeader(0x1D, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, value);
}

void MVDStdService::setConfig(u32 messagePointer) {
	const u32 id = mem.read32(messagePointer + 4);
	const u32 value = mem.read32(messagePointer + 8);
	log("mvd:std::SetConfig (id=%u, value=%u)\n", id, value);

	switch (id) {
		case 0: currentWidth = value; break;
		case 1: currentHeight = value; break;
		case 2: outputLuma = value; break;
		case 3: outputChroma = value; break;
		default: break;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::setOutputBuffer(u32 messagePointer) {
	outputLuma = mem.read32(messagePointer + 4);
	outputChroma = mem.read32(messagePointer + 8);
	log("mvd:std::SetOutputBuffer (luma=%08X chroma=%08X)\n", outputLuma, outputChroma);

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::overrideOutputBuffers(u32 messagePointer) {
	outputLuma = mem.read32(messagePointer + 4);
	outputChroma = mem.read32(messagePointer + 8);
	log("mvd:std::OverrideOutputBuffers (luma=%08X chroma=%08X)\n", outputLuma, outputChroma);

	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MVDStdService::stubCommand(u32 messagePointer, const char* name) {
	const u32 command = mem.read32(messagePointer);
	log("mvd:std::%s (command=%08X) [stubbed]\n", name, command);
	mem.write32(messagePointer, IPC::responseHeader(command >> 16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
