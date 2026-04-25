#include "../../../include/services/reference_fallback_bridge.hpp"

#include <string>

#include "../../../include/ipc.hpp"
#include "../../../include/memory.hpp"

namespace {
// Optional externally-provided callbacks.
// Signature intentionally uses only C ABI + primitive/std types so reference-side integration
// can be implemented without pulling heavy dependencies into Aurora.
using ServiceFallbackFn = bool (*)(void* memoryCtx, u32 serviceHandle, u32 messagePointer);
using PortFallbackFn = bool (*)(void* memoryCtx, const char* portName, u32 messagePointer);

extern "C" ServiceFallbackFn NBAReferenceFallbackHandleService __attribute__((weak));
extern "C" PortFallbackFn NBAReferenceFallbackHandlePort __attribute__((weak));

class ReferenceFallbackBridge final : public HLEFallbackBridge {
  public:
	explicit ReferenceFallbackBridge(Memory& mem) : mem(mem) {}

	bool handleUnknownService(HorizonHandle serviceHandle, u32 messagePointer) override {
		if (NBAReferenceFallbackHandleService == nullptr) {
			return false;
		}
		return NBAReferenceFallbackHandleService(static_cast<void*>(&mem), serviceHandle, messagePointer);
	}

	bool handleUnsupportedPort(std::string_view portName, u32 messagePointer) override {
		if (NBAReferenceFallbackHandlePort == nullptr) {
			return false;
		}
		return NBAReferenceFallbackHandlePort(static_cast<void*>(&mem), std::string(portName).c_str(), messagePointer);
	}

  private:
	Memory& mem;
};
}  // namespace

std::unique_ptr<HLEFallbackBridge> CreateReferenceFallbackBridge(Memory& mem) {
	return std::make_unique<ReferenceFallbackBridge>(mem);
}
