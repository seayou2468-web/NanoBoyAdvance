#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>

// Circular dependencies, yay
class Kernel;

class MICService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::MIC;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, micLogger)

	// Service commands
	void getEventHandle(u32 messagePointer);
	void getGain(u32 messagePointer);
	void getPower(u32 messagePointer);
	void isSampling(u32 messagePointer);
	void mapSharedMem(u32 messagePointer);
	void setClamp(u32 messagePointer);
	void setGain(u32 messagePointer);
	void setIirFilter(u32 messagePointer);
	void setPower(u32 messagePointer);
	void startSampling(u32 messagePointer);
	void stopSampling(u32 messagePointer);
	void unmapSharedMem(u32 messagePointer);
	void theCaptainToadFunction(u32 messagePointer);

	u8 gain = 0;  // How loud our microphone input signal is
	bool micEnabled = false;
	bool shouldClamp = false;
	bool currentlySampling = false;
	u32 mappedSharedMemHandle = 0;
	u32 mappedSharedMemSize = 0;
	u32 sampleOffset = 0;
	u32 sampleDataSize = 0;
	u8 sampleEncoding = 0;
	u8 sampleRate = 0;
	bool sampleLoop = false;
	std::array<u8, 128> iirFilterData {};

	std::optional<Handle> eventHandle;

  public:
	MICService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
