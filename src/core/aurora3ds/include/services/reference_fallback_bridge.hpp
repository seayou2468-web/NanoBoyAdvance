#pragma once

#include <memory>

#include "./hle_fallback_bridge.hpp"

class Memory;

// Creates a bridge that can forward unimplemented Aurora HLE IPC requests to an external
// reference runtime when linked in. If no reference callbacks are present, it remains inert.
std::unique_ptr<HLEFallbackBridge> CreateReferenceFallbackBridge(Memory& mem);

