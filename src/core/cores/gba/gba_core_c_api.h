#pragma once

#include "../../emulator_core_c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef EmulatorCoreHandle GBACoreHandle;

typedef enum GBAKey {
  GBA_KEY_A = EMULATOR_KEY_A,
  GBA_KEY_B = EMULATOR_KEY_B,
  GBA_KEY_SELECT = EMULATOR_KEY_SELECT,
  GBA_KEY_START = EMULATOR_KEY_START,
  GBA_KEY_RIGHT = EMULATOR_KEY_RIGHT,
  GBA_KEY_LEFT = EMULATOR_KEY_LEFT,
  GBA_KEY_UP = EMULATOR_KEY_UP,
  GBA_KEY_DOWN = EMULATOR_KEY_DOWN,
  GBA_KEY_R = EMULATOR_KEY_R,
  GBA_KEY_L = EMULATOR_KEY_L,
} GBAKey;

GBACoreHandle* GBA_Create(void);
void GBA_Destroy(GBACoreHandle* handle);

bool GBA_LoadBIOSFromPath(GBACoreHandle* handle, const char* bios_path);
bool GBA_LoadROMFromPath(GBACoreHandle* handle, const char* rom_path);
void GBA_StepFrame(GBACoreHandle* handle);
void GBA_SetKeyStatus(GBACoreHandle* handle, GBAKey key, bool pressed);

const uint32_t* GBA_GetFrameBufferRGBA(GBACoreHandle* handle, size_t* pixel_count);
const char* GBA_GetLastError(GBACoreHandle* handle);

#ifdef __cplusplus
}
#endif
