#pragma once

#include <array>
#include <optional>
#include <unordered_map>

#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class Kernel;

// Generic stateful fallback used for unknown/partially implemented OS services.
// This is intentionally deterministic and avoids external dependencies.
class OSExtService {
	using Handle = HorizonHandle;

	struct ServiceState {
		std::array<u32, 64> registers{};
		std::unordered_map<u32, std::array<u8, 256>> blobStore;
		u32 flags = 0;
		u32 version = 0x00010000;
	};

	Memory& mem;
	Kernel& kernel;
	ServiceState state;
	std::optional<Handle> eventHandle = std::nullopt;

	MAKE_LOG_FUNCTION(log, srvLogger)

	enum class GenericCommand : u32 {
		Initialize = 0x0001,
		GetState = 0x0002,
		SetState = 0x0003,
		ReadRegister = 0x0004,
		WriteRegister = 0x0005,
		GetVersion = 0x0006,
		Reset = 0x0007,
		GetEventHandle = 0x0008,
		ReadBlob = 0x0009,
		WriteBlob = 0x000A,
		GetTicks = 0x000B,
	};

	void respondDefault(u32 messagePointer, u32 commandID);
	void initialize(u32 messagePointer);
	void getState(u32 messagePointer);
	void setState(u32 messagePointer);
	void readRegister(u32 messagePointer);
	void writeRegister(u32 messagePointer);
	void getVersion(u32 messagePointer);
	void resetState(u32 messagePointer);
	void getEvent(u32 messagePointer);
	void readBlob(u32 messagePointer);
	void writeBlob(u32 messagePointer);
	void getTicks(u32 messagePointer);

  public:
	OSExtService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};

