#include "core_adapter.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace core {

extern const CoreAdapter kGBAAdapter;
extern const CoreAdapter kQuickNesAdapter;
extern const CoreAdapter kSameBoyAdapter;
extern const CoreAdapter kMelonDSAdapter;

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
  switch (type) {
    case EMULATOR_CORE_TYPE_GBA:
      return &kGBAAdapter;
    case EMULATOR_CORE_TYPE_NES:
      return &kQuickNesAdapter;
    case EMULATOR_CORE_TYPE_GB:
      return &kSameBoyAdapter;
    case EMULATOR_CORE_TYPE_NDS:
      return &kMelonDSAdapter;
#if defined(__APPLE__) && TARGET_OS_IPHONE
    case EMULATOR_CORE_TYPE_3DS:
      return GetMikageAdapter();
#endif
    default:
      return nullptr;
  }
}

}  // namespace core
