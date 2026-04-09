#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__linux__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#error "gba_core_c_api supports only iOS and Linux targets"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GBACoreHandle GBACoreHandle;

typedef enum GBAKey {
  GBA_KEY_A = 0,
  GBA_KEY_B = 1,
  GBA_KEY_SELECT = 2,
  GBA_KEY_START = 3,
  GBA_KEY_RIGHT = 4,
  GBA_KEY_LEFT = 5,
  GBA_KEY_UP = 6,
  GBA_KEY_DOWN = 7,
  GBA_KEY_R = 8,
  GBA_KEY_L = 9,
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
