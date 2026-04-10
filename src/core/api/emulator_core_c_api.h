#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__linux__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#error "emulator_core_c_api supports only iOS and Linux targets"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmulatorCoreHandle EmulatorCoreHandle;

typedef enum EmulatorPixelFormat {
  EMULATOR_PIXEL_FORMAT_ARGB8888 = 0,
  EMULATOR_PIXEL_FORMAT_RGBA8888 = 1,
} EmulatorPixelFormat;

typedef enum EmulatorCoreType {
  EMULATOR_CORE_TYPE_GBA = 0,
  EMULATOR_CORE_TYPE_NES = 1,
  EMULATOR_CORE_TYPE_GB = 2,
} EmulatorCoreType;

typedef enum EmulatorKey {
  EMULATOR_KEY_A = 0,
  EMULATOR_KEY_B = 1,
  EMULATOR_KEY_SELECT = 2,
  EMULATOR_KEY_START = 3,
  EMULATOR_KEY_RIGHT = 4,
  EMULATOR_KEY_LEFT = 5,
  EMULATOR_KEY_UP = 6,
  EMULATOR_KEY_DOWN = 7,
  EMULATOR_KEY_R = 8,
  EMULATOR_KEY_L = 9,
} EmulatorKey;

typedef struct EmulatorVideoSpec {
  uint32_t width;
  uint32_t height;
  EmulatorPixelFormat pixel_format;
} EmulatorVideoSpec;

// Returns a stable core identifier (e.g. "nanoboyadvance") for logging/UI labels.
const char* EmulatorCoreTypeName(EmulatorCoreType core_type);

EmulatorCoreHandle* EmulatorCore_Create(EmulatorCoreType core_type);
void EmulatorCore_Destroy(EmulatorCoreHandle* handle);

bool EmulatorCore_LoadBIOSFromPath(EmulatorCoreHandle* handle, const char* bios_path);
bool EmulatorCore_LoadROMFromPath(EmulatorCoreHandle* handle, const char* rom_path);
bool EmulatorCore_LoadROMFromMemory(EmulatorCoreHandle* handle, const void* rom_data, size_t rom_size);
void EmulatorCore_StepFrame(EmulatorCoreHandle* handle);
void EmulatorCore_SetKeyStatus(EmulatorCoreHandle* handle, EmulatorKey key, bool pressed);

bool EmulatorCore_GetVideoSpec(const EmulatorCoreHandle* handle, EmulatorVideoSpec* out_spec);
const uint32_t* EmulatorCore_GetFrameBufferRGBA(EmulatorCoreHandle* handle, size_t* pixel_count);
const char* EmulatorCore_GetLastError(EmulatorCoreHandle* handle);

// Save/load state as raw binary blob owned by the caller.
// Pass out_buffer=null to query required size in out_size.
bool EmulatorCore_SaveStateToBuffer(
    EmulatorCoreHandle* handle, void* out_buffer, size_t buffer_size, size_t* out_size);
bool EmulatorCore_LoadStateFromBuffer(
    EmulatorCoreHandle* handle, const void* state_buffer, size_t state_size);

// Apply a cheat code string.
// Current implementation supports NES RAM-poke format: "ram:HHHH=VV".
bool EmulatorCore_ApplyCheatCode(EmulatorCoreHandle* handle, const char* cheat_code);

#ifdef __cplusplus
}
#endif
