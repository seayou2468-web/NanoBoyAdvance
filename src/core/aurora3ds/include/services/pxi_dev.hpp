#pragma once
#include "../helpers.hpp"
#include "../kernel/handles.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include <array>
#include <vector>

class PXIDevService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::PXI_DEV;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, pxiLogger)
	std::array<u32, 16> hostIO {};
	std::vector<u8> hostExBuffer {};
	bool midiInitialized = false;
	std::array<u8, 256> midiBuffer {};
	u32 midiReadOffset = 0;
	u32 cardDeviceID = 0;

	void readHostIO(u32 messagePointer);
	void writeHostIO(u32 messagePointer);
	void readHostEx(u32 messagePointer);
	void writeHostEx(u32 messagePointer);
	void writeHostExStart(u32 messagePointer);
	void writeHostExChunk(u32 messagePointer);
	void writeHostExEnd(u32 messagePointer);
	void initializeMIDI(u32 messagePointer);
	void finalizeMIDI(u32 messagePointer);
	void getMIDIInfo(u32 messagePointer);
	void getMIDIBufferSize(u32 messagePointer);
	void readMIDI(u32 messagePointer);
	void spiMultiWriteRead(u32 messagePointer);
	void spiWriteRead(u32 messagePointer);
	void getCardDevice(u32 messagePointer);
	void writeSimpleResult(u32 messagePointer, u32 commandID, u32 words = 1);

  public:
	explicit PXIDevService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
