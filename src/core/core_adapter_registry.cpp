#include "./core_adapter.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// Build Mikage adapter only for iOS frontend in the current repository layout.
#if defined(__APPLE__) && TARGET_OS_IPHONE
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 1
#include "./aurora3ds/core_adapter.cpp"
#endif

namespace core {

#if defined(__APPLE__) && TARGET_OS_IPHONE
extern const CoreAdapter kMikageAdapter;
#endif

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
  if (type == EMULATOR_CORE_TYPE_3DS) {
    return &kMikageAdapter;
  }
#endif
  return nullptr;
}

}  // namespace core
