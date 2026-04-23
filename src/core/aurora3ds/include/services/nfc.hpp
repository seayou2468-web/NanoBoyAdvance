#pragma once
#include <filesystem>

#include "./amiibo_device.hpp"
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"
#include <array>

// You know the drill
class Kernel;

class NFCService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::NFC;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, nfcLogger)

	enum class Old3DSAdapterStatus : u32 {
		Idle = 0,
		AttemptingToInitialize = 1,
		InitializationComplete = 2,
		Active = 3,
	};

	enum class TagStatus : u8 {
		NotInitialized = 0,
		Initialized = 1,
		Scanning = 2,
		InRange = 3,
		OutOfRange = 4,
		Loaded = 5,
	};

	// Kernel events signaled when an NFC tag goes in and out of range respectively
	std::optional<Handle> tagInRangeEvent, tagOutOfRangeEvent;

	AmiiboDevice device;
	Old3DSAdapterStatus adapterStatus;
	TagStatus tagStatus;
	bool initialized = false;
	bool appAreaCreated = false;
	bool appAreaOpen = false;
	u32 appAreaID = 0;
	u16 appAreaSize = 0;
	std::array<u8, 0xD8> appAreaData {};

	// Service commands
	void communicationGetResult(u32 messagePointer);
	void communicationGetStatus(u32 messagePointer);
	void initialize(u32 messagePointer);
	void getModelInfo(u32 messagePointer);
	void getTagInfo(u32 messagePointer);
	void getTagInRangeEvent(u32 messagePointer);
	void getTagOutOfRangeEvent(u32 messagePointer);
	void getTagState(u32 messagePointer);
	void loadAmiiboPartially(u32 messagePointer);
	void mount(u32 messagePointer);
	void unmount(u32 messagePointer);
	void flush(u32 messagePointer);
	void openApplicationArea(u32 messagePointer);
	void createApplicationArea(u32 messagePointer);
	void readApplicationArea(u32 messagePointer);
	void writeApplicationArea(u32 messagePointer);
	void getNfpRegisterInfo(u32 messagePointer);
	void getNfpCommonInfo(u32 messagePointer);
	void initializeCreateInfo(u32 messagePointer);
	void getIdentificationBlock(u32 messagePointer);
	void format(u32 messagePointer);
	void getAdminInfo(u32 messagePointer);
	void getEmptyRegisterInfo(u32 messagePointer);
	void setRegisterInfo(u32 messagePointer);
	void deleteRegisterInfo(u32 messagePointer);
	void deleteApplicationArea(u32 messagePointer);
	void existsApplicationArea(u32 messagePointer);
	void shutdown(u32 messagePointer);
	void startCommunication(u32 messagePointer);
	void startTagScanning(u32 messagePointer);
	void stopCommunication(u32 messagePointer);
	void stopTagScanning(u32 messagePointer);

  public:
	NFCService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	bool loadAmiibo(const std::filesystem::path& path);
};
