#include "../../../include/services/hid.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include "../../../include/ipc.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../include/services/ir/circlepad_pro.hpp"

namespace HIDCommands {
	enum : u32 {
		CalibrateTouchScreen = 0x00010140,
		UpdateTouchConfig = 0x00020080,
		GetIPCHandles = 0x000A0000,
		EnableAccelerometer = 0x00110000,
		DisableAccelerometer = 0x00120000,
		EnableGyroscopeLow = 0x00130000,
		DisableGyroscopeLow = 0x00140000,
		GetGyroscopeLowRawToDpsCoefficient = 0x00150000,
		GetGyroscopeLowCalibrateParam = 0x00160000,
		GetSoundVolume = 0x00170000,
	};
}

void HIDService::reset() {
	sharedMem = nullptr;
	accelerometerEnabled = false;
	eventsInitialized = false;
	gyroEnabled = false;
	touchScreenPressed = false;
	for (auto& e : events) e = std::nullopt;
	nextPadIndex = nextTouchscreenIndex = nextAccelerometerIndex = nextGyroIndex = 0;
	newButtons = oldButtons = 0;
	circlePadX = circlePadY = 0;
	touchScreenX = touchScreenY = 0;
	roll = pitch = yaw = 0;
	accelX = accelY = accelZ = 0;
	cStickX = cStickY = IR::CirclePadPro::ButtonState::C_STICK_CENTER;
}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	IPC::RequestParser rp(messagePointer, mem);
	switch (command) {
		case HIDCommands::GetIPCHandles: getIPCHandles(messagePointer); break;
		case HIDCommands::EnableAccelerometer: enableAccelerometer(messagePointer); break;
		case HIDCommands::DisableAccelerometer: disableAccelerometer(messagePointer); break;
		case HIDCommands::EnableGyroscopeLow: enableGyroscopeLow(messagePointer); break;
		case HIDCommands::DisableGyroscopeLow: disableGyroscopeLow(messagePointer); break;
		case HIDCommands::GetGyroscopeLowRawToDpsCoefficient: getGyroscopeCoefficient(messagePointer); break;
		case HIDCommands::GetGyroscopeLowCalibrateParam: getGyroscopeLowCalibrateParam(messagePointer); break;
		case HIDCommands::GetSoundVolume: getSoundVolume(messagePointer); break;
		default:
			log("HID unknown command: %08X\n", command);
			auto rb = rp.MakeBuilder(1, 0);
			rb.Push(Result::Success);
			break;
	}
}

void HIDService::enableAccelerometer(u32 messagePointer) {
	log("HID::EnableAccelerometer\n");
	accelerometerEnabled = true;
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void HIDService::disableAccelerometer(u32 messagePointer) {
	log("HID::DisableAccelerometer\n");
	accelerometerEnabled = false;
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void HIDService::enableGyroscopeLow(u32 messagePointer) {
	log("HID::EnableGyroscopeLow\n");
	gyroEnabled = true;
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void HIDService::disableGyroscopeLow(u32 messagePointer) {
	log("HID::DisableGyroscopeLow\n");
	gyroEnabled = false;
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 0);
	rb.Push(Result::Success);
}

void HIDService::getGyroscopeLowCalibrateParam(u32 messagePointer) {
	log("HID::GetGyroscopeLowCalibrateParam\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(6, 0);
	rb.Push(Result::Success);
	for(int i=0; i<5; ++i) rb.Push(0u);
}

void HIDService::getGyroscopeCoefficient(u32 messagePointer) {
	log("HID::GetGyroscopeCoefficient\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(0x41660000u); // 14.375f as u32
}

void HIDService::getSoundVolume(u32 messagePointer) {
	log("HID::GetSoundVolume\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(2, 0);
	rb.Push(Result::Success);
	rb.Push(63u);
}

void HIDService::getIPCHandles(u32 messagePointer) {
	log("HID::GetIPCHandles\n");
	IPC::RequestParser rp(messagePointer, mem);
	auto rb = rp.MakeBuilder(1, 4);
	rb.Push(Result::Success);
	rb.Push(IPC::CopyHandleDesc(1));
	rb.Push(KernelHandles::HIDSharedMemHandle);
}

void HIDService::updateInputs(u64 timestamp) {}
