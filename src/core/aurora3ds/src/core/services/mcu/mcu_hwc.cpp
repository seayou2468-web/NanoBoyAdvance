#include "../../../../include/services/mcu/mcu_hwc.hpp"
#include "../../../../include/ipc.hpp"

namespace MCUCommands {
	enum : u32 {
		ReadRegister = 0x00010040,
		WriteRegister = 0x00020080,
		GetBatteryVoltage = 0x00040000,
		GetBatteryLevel = 0x00050000,
		SetPowerLEDPattern = 0x00060040,
		GetSoundVolume = 0x000B0000,
		GetMcuFwVerHigh = 0x00100000,
		GetMcuFwVerLow = 0x00110000,
	};
}

void MCU::HWCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);

	switch (command) {
		case MCUCommands::GetBatteryLevel: {
			log("MCU:HWC: GetBatteryLevel\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(5u); // Max level
			break;
		}
		case MCUCommands::GetBatteryVoltage: {
			log("MCU:HWC: GetBatteryVoltage\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(4100u); // 4.1V
			break;
		}
		case MCUCommands::GetSoundVolume: {
			log("MCU:HWC: GetSoundVolume\n");
			auto rb = rp.MakeBuilder(2, 0);
			rb.Push(Result::Success);
			rb.Push(63u); // Max volume
			break;
		}
		default:
			log("MCU:HWC unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}
