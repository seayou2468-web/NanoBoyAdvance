#pragma once
#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include <array>
#include <random>

class PSPSService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::PS_PS;
	Memory& mem;
	std::mt19937 rng;
	u32 deviceID = 0;
	u64 localFriendCodeSeed = 0;
	std::array<u32, 4> pxiInterfaceRegs {};
	MAKE_LOG_FUNCTION(log, srvLogger)

	void generateRandomBytes(u32 messagePointer);
	void seedRNG(u32 messagePointer);
	void getCTRCardAutoStartupBit(u32 messagePointer);
	void getLocalFriendCodeSeed(u32 messagePointer);
	void getDeviceID(u32 messagePointer);
	void getRomID(u32 messagePointer, bool secondary);
	void getRomMakerCode(u32 messagePointer);
	void signRsaSha256(u32 messagePointer);
	void verifyRsaSha256(u32 messagePointer);
	void encryptDecryptAes(u32 messagePointer);
	void encryptSignDecryptVerifyAesCcm(u32 messagePointer);
	void pxiInterface04010084(u32 messagePointer);
	void pxiInterface04020082(u32 messagePointer);
	void pxiInterface04030044(u32 messagePointer);
	void pxiInterface04040044(u32 messagePointer);
	void stubCommand(u32 messagePointer, const char* name);

  public:
	PSPSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
