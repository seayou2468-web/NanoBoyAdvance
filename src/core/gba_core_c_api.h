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

GBACoreHandle* GBA_Create(void);
void GBA_Destroy(GBACoreHandle* handle);

bool GBA_LoadROMFromPath(GBACoreHandle* handle, const char* rom_path);
void GBA_StepFrame(GBACoreHandle* handle);

const uint32_t* GBA_GetFrameBufferRGBA(GBACoreHandle* handle, size_t* pixel_count);
const char* GBA_GetLastError(GBACoreHandle* handle);

#ifdef __cplusplus
}
#endif
