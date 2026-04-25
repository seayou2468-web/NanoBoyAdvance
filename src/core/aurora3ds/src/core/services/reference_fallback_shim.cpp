#include "../../../include/ipc.hpp"
#include "../../../include/memory.hpp"
#include "../../../include/result/result.hpp"

// Temporary in-process compatibility shim.
// If an external reference runtime is linked and provides strong definitions for these symbols,
// they override these weak defaults automatically.

extern "C" bool __attribute__((weak)) NBAReferenceFallbackHandleService(void* memoryCtx, u32 serviceHandle, u32 messagePointer) {
	if (memoryCtx == nullptr) {
		return false;
	}

	auto& mem = *static_cast<Memory*>(memoryCtx);
	const u32 commandWord = mem.read32(messagePointer);
	const u32 commandId = commandWord >> 16;

	Helpers::warn(
		"[REF-FALLBACK] service handle=%08X cmd=%08X id=%04X (temporary shim)",
		serviceHandle, commandWord, commandId
	);

	// Generic success-style response so unsupported calls can keep progressing while Aurora HLE
	// coverage is being expanded.
	mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	return true;
}

extern "C" bool __attribute__((weak)) NBAReferenceFallbackHandlePort(void* memoryCtx, const char* portName, u32 messagePointer) {
	if (memoryCtx == nullptr) {
		return false;
	}

	auto& mem = *static_cast<Memory*>(memoryCtx);
	const u32 commandWord = mem.read32(messagePointer);
	const u32 commandId = commandWord >> 16;

	Helpers::warn(
		"[REF-FALLBACK] port=%s cmd=%08X id=%04X (temporary shim)",
		(portName != nullptr) ? portName : "<null>", commandWord, commandId
	);

	mem.write32(messagePointer, IPC::responseHeader(commandId, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	return true;
}

