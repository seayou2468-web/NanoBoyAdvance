#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class MVDStdService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::MVD_STD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, srvLogger)
	bool initialized = false;
	u32 status = 0;
	u32 frameCounter = 0;
	u32 currentWidth = 0;
	u32 currentHeight = 0;
	u32 outputLuma = 0;
	u32 outputChroma = 0;

	void initialize(u32 messagePointer);
	void shutdown(u32 messagePointer);
	void calculateWorkBufSize(u32 messagePointer);
	void calculateImageSize(u32 messagePointer);
	void processNALUnit(u32 messagePointer);
	void controlFrameRendering(u32 messagePointer);
	void getStatus(u32 messagePointer, u32 commandID);
	void getConfig(u32 messagePointer);
	void setConfig(u32 messagePointer);
	void setOutputBuffer(u32 messagePointer);
	void overrideOutputBuffers(u32 messagePointer);
	void stubCommand(u32 messagePointer, const char* name);

  public:
	MVDStdService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
