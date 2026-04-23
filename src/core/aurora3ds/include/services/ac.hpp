#pragma once
#include <optional>
#include <array>

#include "../helpers.hpp"
#include "../kernel/kernel_types.hpp"
#include "../logger.hpp"
#include "../memory.hpp"
#include "../result/result.hpp"

class ACService {
	using Handle = HorizonHandle;

	Handle handle = KernelHandles::AC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, acLogger)

	struct ACConfig {
		std::array<u8, 0x200> data;
	};

	enum class Status : u32 {
		Disconnected = 0,
		Enabled = 1,
		Connected = 2,
		Internet = 3,
	};

	enum class WifiStatus : u32 {
		Disconnected = 0,
		ConnectedSlot1 = (1 << 0),
		ConnectedSlot2 = (1 << 1),
		ConnectedSlot3 = (1 << 2),
	};

	// Service commands
	void createDefaultConfig(u32 messagePointer);
	void connectAsync(u32 messagePointer);
	void getConnectResult(u32 messagePointer);
	void cancelConnectAsync(u32 messagePointer);
	void closeAsync(u32 messagePointer);
	void getCloseResult(u32 messagePointer);
	void getStatus(u32 messagePointer);
	void getWifiStatus(u32 messagePointer);
	void getInfraPriority(u32 messagePointer);
	void setRequestEulaVersion(u32 messagePointer);
	void getNZoneBeaconNotFoundEvent(u32 messagePointer);
	void registerDisconnectEvent(u32 messagePointer);
	void getConnectingProxyEnable(u32 messagePointer);
	void isConnected(u32 messagePointer);
	void setClientVersion(u32 messagePointer);

	bool connected = false;
	std::optional<Handle> disconnectEvent = std::nullopt;
	ACConfig defaultConfig {};

  public:
	ACService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
