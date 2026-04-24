#include "../../../include/services/pxi_dev.hpp"

#include <algorithm>

#include "../../../include/ipc.hpp"

void PXIDevService::reset() {
	hostIO.fill(0);
	hostExBuffer.clear();
	midiInitialized = false;
	midiBuffer.fill(0);
	midiReadOffset = 0;
	cardDeviceID = 0;
}

void PXIDevService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;

	switch (commandID) {
		case 0x0001: readHostIO(messagePointer); return;
		case 0x0002: writeHostIO(messagePointer); return;
		case 0x0003: readHostEx(messagePointer); return;
		case 0x0004: writeHostEx(messagePointer); return;
		case 0x0005: writeHostExStart(messagePointer); return;
		case 0x0006: writeHostExChunk(messagePointer); return;
		case 0x0007: writeHostExEnd(messagePointer); return;
		case 0x0008: initializeMIDI(messagePointer); return;
		case 0x0009: finalizeMIDI(messagePointer); return;
		case 0x000A: getMIDIInfo(messagePointer); return;
		case 0x000B: getMIDIBufferSize(messagePointer); return;
		case 0x000C: readMIDI(messagePointer); return;
		case 0x000D: spiMultiWriteRead(messagePointer); return;
		case 0x000E: spiWriteRead(messagePointer); return;
		case 0x000F: getCardDevice(messagePointer); return;
	}

	Helpers::panic("PXI:DEV service requested. Command: %08X\n", command);
}

void PXIDevService::writeSimpleResult(u32 messagePointer, u32 commandID, u32 words) {
	mem.write32(messagePointer, IPC::responseHeader(commandID, words, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PXIDevService::readHostIO(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4) & 0xF;
	log("PXI:DEV::ReadHostIO(index=%u)\n", index);
	mem.write32(messagePointer, IPC::responseHeader(0x1, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, hostIO[index]);
}

void PXIDevService::writeHostIO(u32 messagePointer) {
	const u32 index = mem.read32(messagePointer + 4) & 0xF;
	hostIO[index] = mem.read32(messagePointer + 8);
	log("PXI:DEV::WriteHostIO(index=%u, value=%08X)\n", index, hostIO[index]);
	writeSimpleResult(messagePointer, 0x2);
}

void PXIDevService::readHostEx(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 out = mem.read32(messagePointer + 12);
	const u32 bytes = std::min<u32>(size, static_cast<u32>(hostExBuffer.size()));
	for (u32 i = 0; i < bytes; i++) {
		mem.write8(out + i, hostExBuffer[i]);
	}
	mem.write32(messagePointer, IPC::responseHeader(0x3, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, bytes);
}

void PXIDevService::writeHostEx(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 in = mem.read32(messagePointer + 12);
	hostExBuffer.resize(size);
	for (u32 i = 0; i < size; i++) {
		hostExBuffer[i] = mem.read8(in + i);
	}
	writeSimpleResult(messagePointer, 0x4);
}

void PXIDevService::writeHostExStart(u32 messagePointer) {
	hostExBuffer.clear();
	writeSimpleResult(messagePointer, 0x5);
}

void PXIDevService::writeHostExChunk(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 in = mem.read32(messagePointer + 12);
	const u32 oldSize = static_cast<u32>(hostExBuffer.size());
	hostExBuffer.resize(oldSize + size);
	for (u32 i = 0; i < size; i++) {
		hostExBuffer[oldSize + i] = mem.read8(in + i);
	}
	writeSimpleResult(messagePointer, 0x6);
}

void PXIDevService::writeHostExEnd(u32 messagePointer) {
	writeSimpleResult(messagePointer, 0x7);
}

void PXIDevService::initializeMIDI(u32 messagePointer) {
	midiInitialized = true;
	midiReadOffset = 0;
	writeSimpleResult(messagePointer, 0x8);
}

void PXIDevService::finalizeMIDI(u32 messagePointer) {
	midiInitialized = false;
	writeSimpleResult(messagePointer, 0x9);
}

void PXIDevService::getMIDIInfo(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xA, 3, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, midiInitialized ? 1 : 0);
	mem.write32(messagePointer + 12, midiReadOffset);
}

void PXIDevService::getMIDIBufferSize(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(midiBuffer.size()));
}

void PXIDevService::readMIDI(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 out = mem.read32(messagePointer + 12);
	const u32 remaining = static_cast<u32>(midiBuffer.size()) - std::min<u32>(midiReadOffset, static_cast<u32>(midiBuffer.size()));
	const u32 bytes = std::min<u32>(size, remaining);
	for (u32 i = 0; i < bytes; i++) {
		mem.write8(out + i, midiBuffer[midiReadOffset + i]);
	}
	midiReadOffset += bytes;
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, bytes);
}

void PXIDevService::spiMultiWriteRead(u32 messagePointer) {
	// Compatibility path: acknowledge operation as successful.
	writeSimpleResult(messagePointer, 0xD);
}

void PXIDevService::spiWriteRead(u32 messagePointer) {
	// Compatibility path: acknowledge operation as successful.
	writeSimpleResult(messagePointer, 0xE);
}

void PXIDevService::getCardDevice(u32 messagePointer) {
	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, cardDeviceID);
}
