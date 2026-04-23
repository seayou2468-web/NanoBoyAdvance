#pragma once
#include <array>

#include "../helpers.hpp"
#include "../kernel/handles.hpp"
#include "../logger.hpp"
#include "../memory.hpp"

class PeripheralBusService {
  public:
	enum class Type {
		CDC,
		GPIO,
		I2C,
		MP,
		PDN,
		SPI,
	};

  private:
	Memory& mem;
	std::array<u32, 256> gpioRegisters{};
	std::array<u32, 256> i2cRegisters{};
	std::array<u32, 256> spiRegisters{};
	u32 cdcFlags = 0;
	u32 mpState = 0;
	u32 pdnWakeMask = 0;
	MAKE_LOG_FUNCTION(log, srvLogger)

	void initialize(u32 messagePointer, Type type);
	void readRegister(u32 messagePointer, Type type);
	void writeRegister(u32 messagePointer, Type type);
	void getState(u32 messagePointer, Type type);
	void setState(u32 messagePointer, Type type);
	void respondNotImplemented(u32 messagePointer, Type type);
	std::array<u32, 256>& getRegisterArray(Type type);
	static const char* getServiceName(Type type);

  public:
	explicit PeripheralBusService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};
