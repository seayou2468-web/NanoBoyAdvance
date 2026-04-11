#include "core_adapter.hpp"

namespace core {

extern const CoreAdapter kGBAAdapter;
extern const CoreAdapter kQuickNesAdapter;
extern const CoreAdapter kSameBoyAdapter;
#if defined(ENABLE_MELONDS_CORE) || defined(__APPLE__)
extern const CoreAdapter kMelonDSAdapter;
#endif

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
  switch (type) {
    case EMULATOR_CORE_TYPE_GBA:
      return &kGBAAdapter;
    case EMULATOR_CORE_TYPE_NES:
      return &kQuickNesAdapter;
    case EMULATOR_CORE_TYPE_GB:
      return &kSameBoyAdapter;
#if defined(ENABLE_MELONDS_CORE) || defined(__APPLE__)
    case EMULATOR_CORE_TYPE_NDS:
      return &kMelonDSAdapter;
#endif
    default:
      return nullptr;
  }
}

}  // namespace core
