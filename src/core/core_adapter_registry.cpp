#include "core_adapter.hpp"

namespace core {

extern const CoreAdapter kGBAAdapter;
extern const CoreAdapter kQuickNesAdapter;
extern const CoreAdapter kSameBoyAdapter;
extern const CoreAdapter kDesumeAdapter;

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
  switch (type) {
    case EMULATOR_CORE_TYPE_GBA:
      return &kGBAAdapter;
    case EMULATOR_CORE_TYPE_NES:
      return &kQuickNesAdapter;
    case EMULATOR_CORE_TYPE_GB:
      return &kSameBoyAdapter;
    case EMULATOR_CORE_TYPE_NDS:
      return &kDesumeAdapter;
    default:
      return nullptr;
  }
}

}  // namespace core
