#include "core_adapter.hpp"

namespace core {

#ifndef CORE_ENABLE_GBA
#define CORE_ENABLE_GBA 1
#endif
#ifndef CORE_ENABLE_NES
#define CORE_ENABLE_NES 1
#endif
#ifndef CORE_ENABLE_GB
#define CORE_ENABLE_GB 1
#endif
#ifndef CORE_ENABLE_NDS
#define CORE_ENABLE_NDS 1
#endif
#ifndef CORE_ENABLE_3DS
#define CORE_ENABLE_3DS 1
#endif

#if CORE_ENABLE_GBA
extern const CoreAdapter kGBAAdapter;
#endif
#if CORE_ENABLE_NES
extern const CoreAdapter kQuickNesAdapter;
#endif
#if CORE_ENABLE_GB
extern const CoreAdapter kSameBoyAdapter;
#endif
#if CORE_ENABLE_NDS
extern const CoreAdapter kMelonDSAdapter;
#endif
#if CORE_ENABLE_3DS
extern const CoreAdapter kCitraAdapter;
#endif

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type) {
  switch (type) {
#if CORE_ENABLE_GBA
    case EMULATOR_CORE_TYPE_GBA:
      return &kGBAAdapter;
#endif
#if CORE_ENABLE_NES
    case EMULATOR_CORE_TYPE_NES:
      return &kQuickNesAdapter;
#endif
#if CORE_ENABLE_GB
    case EMULATOR_CORE_TYPE_GB:
      return &kSameBoyAdapter;
#endif
#if CORE_ENABLE_NDS
    case EMULATOR_CORE_TYPE_NDS:
      return &kMelonDSAdapter;
#endif
#if CORE_ENABLE_3DS
    case EMULATOR_CORE_TYPE_3DS:
      return &kCitraAdapter;
#endif
    default:
      return nullptr;
  }
}

}  // namespace core
