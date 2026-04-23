#include "../../../include/services/peripheral_bus.hpp"

#include "../../../include/ipc.hpp"

namespace PeripheralBusCommands {
	enum : u32 {
		Initialize = 0x00010000,
		ReadRegister = 0x00020040,
		WriteRegister = 0x00030080,
		GetState = 0x00040000,
		SetState = 0x00050040,
	};
}

void PeripheralBusService::reset() {
	gpioRegisters.fill(0);
	i2cRegisters.fill(0);
	spiRegisters.fill(0);
	cdcFlags = 0;
	mpState = 0;
	pdnWakeMask = 0;
}

const char* PeripheralBusService::getServiceName(Type type) {
	switch (type) {
		case Type::CDC: return "cdc:U";
		case Type::GPIO: return "gpio:U";
		case Type::I2C: return "i2c:U";
		case Type::MP: return "mp:U";
		case Type::PDN: return "pdn:U";
		case Type::SPI: return "spi:U";
		default: return "unknown";
	}
}

void PeripheralBusService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case PeripheralBusCommands::Initialize: initialize(messagePointer, type); break;
		case PeripheralBusCommands::ReadRegister: readRegister(messagePointer, type); break;
		case PeripheralBusCommands::WriteRegister: writeRegister(messagePointer, type); break;
		case PeripheralBusCommands::GetState: getState(messagePointer, type); break;
		case PeripheralBusCommands::SetState: setState(messagePointer, type); break;
		default: respondNotImplemented(messagePointer, type); break;
	}
}

std::array<u32, 256>& PeripheralBusService::getRegisterArray(Type type) {
	switch (type) {
		case Type::GPIO: return gpioRegisters;
		case Type::I2C: return i2cRegisters;
		case Type::SPI: return spiRegisters;
		default: return gpioRegisters;
	}
}

void PeripheralBusService::initialize(u32 messagePointer, Type type) {
	log("%s::Initialize\n", getServiceName(type));
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PeripheralBusService::readRegister(u32 messagePointer, Type type) {
	const u32 index = mem.read32(messagePointer + 4) & 0xFF;
	const auto& registers = getRegisterArray(type);
	const u32 value = registers[index];
	log("%s::ReadRegister(index=%u, value=%08X)\n", getServiceName(type), index, value);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, value);
}

void PeripheralBusService::writeRegister(u32 messagePointer, Type type) {
	const u32 index = mem.read32(messagePointer + 4) & 0xFF;
	const u32 value = mem.read32(messagePointer + 8);
	auto& registers = getRegisterArray(type);
	registers[index] = value;
	log("%s::WriteRegister(index=%u, value=%08X)\n", getServiceName(type), index, value);

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PeripheralBusService::getState(u32 messagePointer, Type type) {
	u32 state = 0;
	switch (type) {
		case Type::CDC: state = cdcFlags; break;
		case Type::MP: state = mpState; break;
		case Type::PDN: state = pdnWakeMask; break;
		case Type::GPIO: state = gpioRegisters[0]; break;
		case Type::I2C: state = i2cRegisters[0]; break;
		case Type::SPI: state = spiRegisters[0]; break;
	}

	log("%s::GetState(state=%08X)\n", getServiceName(type), state);
	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, state);
}

void PeripheralBusService::setState(u32 messagePointer, Type type) {
	const u32 state = mem.read32(messagePointer + 4);
	switch (type) {
		case Type::CDC: cdcFlags = state; break;
		case Type::MP: mpState = state; break;
		case Type::PDN: pdnWakeMask = state; break;
		case Type::GPIO: gpioRegisters[0] = state; break;
		case Type::I2C: i2cRegisters[0] = state; break;
		case Type::SPI: spiRegisters[0] = state; break;
	}

	log("%s::SetState(state=%08X)\n", getServiceName(type), state);
	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void PeripheralBusService::respondNotImplemented(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	const u32 commandID = command >> 16;
	log("%s command %04X (not implemented)\n", getServiceName(type), commandID);
	mem.write32(messagePointer, IPC::responseHeader(commandID, 1, 0));
	mem.write32(messagePointer + 4, Result::OS::NotImplemented);
}
