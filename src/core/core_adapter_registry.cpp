#include "./core_adapter.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(NBA_BUILD_AURORA3DS_ADAPTER)
#define NBA_BUILD_AURORA3DS_ADAPTER 0
#endif

// Build the adapter only when explicitly enabled by build configuration.
#if NBA_BUILD_AURORA3DS_ADAPTER
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 1
#include "./aurora3ds/core_adapter.cpp"
#endif

namespace core {

#if NBA_BUILD_AURORA3DS_ADAPTER
extern const CoreAdapter kMikageAdapter;
#endif

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
#if NBA_BUILD_AURORA3DS_ADAPTER
  if (type == EMULATOR_CORE_TYPE_3DS) {
    return &kMikageAdapter;
  }
#endif
  return nullptr;
}

}  // namespace core
