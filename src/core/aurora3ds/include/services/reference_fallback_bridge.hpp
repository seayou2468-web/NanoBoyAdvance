#pragma once

#include <memory>

#include "./hle_fallback_bridge.hpp"

class Memory;

// C ABI contract for reference HLE integration.
// The reference side should register concrete handlers during process startup.
using NBAReferenceServiceFallbackFn = bool (*)(void* memoryCtx, u32 serviceHandle, u32 messagePointer);
using NBAReferencePortFallbackFn = bool (*)(void* memoryCtx, const char* portName, u32 messagePointer);

extern "C" void NBAReferenceFallbackRegisterHandlers(
	NBAReferenceServiceFallbackFn serviceHandler,
	NBAReferencePortFallbackFn portHandler
);
extern "C" void NBAReferenceFallbackClearHandlers();
bool NBAReferenceFallbackHasHandlers();

// Creates a bridge that forwards Aurora IPC requests to registered reference HLE handlers.
std::unique_ptr<HLEFallbackBridge> CreateReferenceFallbackBridge(Memory& mem);
