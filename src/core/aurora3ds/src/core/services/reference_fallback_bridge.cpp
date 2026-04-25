#include "../../../include/services/reference_fallback_bridge.hpp"

#include <atomic>
#include <string>

#include "../../../include/ipc.hpp"
#include "../../../include/memory.hpp"

namespace {
std::atomic<NBAReferenceServiceFallbackFn> gServiceHandler = nullptr;
std::atomic<NBAReferencePortFallbackFn> gPortHandler = nullptr;

class ReferenceFallbackBridge final : public HLEFallbackBridge {
  public:
	explicit ReferenceFallbackBridge(Memory& mem) : mem(mem) {}

	bool handleUnknownService(HorizonHandle serviceHandle, u32 messagePointer) override {
		const auto serviceHandler = gServiceHandler.load(std::memory_order_acquire);
		if (serviceHandler == nullptr) {
			return false;
		}
		return serviceHandler(static_cast<void*>(&mem), serviceHandle, messagePointer);
	}

	bool handleUnsupportedPort(std::string_view portName, u32 messagePointer) override {
		const auto portHandler = gPortHandler.load(std::memory_order_acquire);
		if (portHandler == nullptr) {
			return false;
		}
		return portHandler(static_cast<void*>(&mem), std::string(portName).c_str(), messagePointer);
	}

  private:
	Memory& mem;
};
}  // namespace

extern "C" void NBAReferenceFallbackRegisterHandlers(
	NBAReferenceServiceFallbackFn serviceHandler,
	NBAReferencePortFallbackFn portHandler
) {
	gServiceHandler.store(serviceHandler, std::memory_order_release);
	gPortHandler.store(portHandler, std::memory_order_release);
}

extern "C" void NBAReferenceFallbackClearHandlers() {
	gServiceHandler.store(nullptr, std::memory_order_release);
	gPortHandler.store(nullptr, std::memory_order_release);
}

bool NBAReferenceFallbackHasHandlers() {
	return gServiceHandler.load(std::memory_order_acquire) != nullptr &&
		gPortHandler.load(std::memory_order_acquire) != nullptr;
}

std::unique_ptr<HLEFallbackBridge> CreateReferenceFallbackBridge(Memory& mem) {
	return std::make_unique<ReferenceFallbackBridge>(mem);
}
