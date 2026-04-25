#pragma once

#include <string_view>

#include "../kernel/kernel_types.hpp"

// Temporary compatibility bridge used while Aurora3DS HLE coverage is still incomplete.
// Implementations can forward unknown service/port IPC to a reference runtime and write the
// response back into Aurora TLS IPC buffers.
class HLEFallbackBridge {
  public:
	virtual ~HLEFallbackBridge() = default;

	// Handle an IPC sync request for an unimplemented service handle.
	// Return true if the bridge consumed the command and wrote a response.
	virtual bool handleUnknownService(HorizonHandle serviceHandle, u32 messagePointer) = 0;

	// Handle an IPC sync request for an unsupported named port session.
	// Return true if consumed.
	virtual bool handleUnsupportedPort(std::string_view portName, u32 messagePointer) = 0;
};

