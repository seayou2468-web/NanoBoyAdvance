#include "core_adapter.hpp"

#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 1
#include "./mikage/core_adapter.cpp"

namespace core {

extern const CoreAdapter kGBAAdapter;
extern const CoreAdapter kQuickNesAdapter;
extern const CoreAdapter kSameBoyAdapter;
extern const CoreAdapter kMelonDSAdapter;
extern const CoreAdapter kMikageAdapter;

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
    case EMULATOR_CORE_TYPE_3DS:
      return &kMikageAdapter;
    default:
      return nullptr;
  }
}

}  // namespace core
