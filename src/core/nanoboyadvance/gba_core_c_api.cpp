#include "gba_core_c_api.h"

extern "C" {

GBACoreHandle* GBA_Create(void) {
  return EmulatorCore_Create(EMULATOR_CORE_TYPE_GBA);
}

void GBA_Destroy(GBACoreHandle* handle) {
  EmulatorCore_Destroy(handle);
}

bool GBA_LoadBIOSFromPath(GBACoreHandle* handle, const char* bios_path) {
  return EmulatorCore_LoadBIOSFromPath(handle, bios_path);
}

bool GBA_LoadROMFromPath(GBACoreHandle* handle, const char* rom_path) {
  return EmulatorCore_LoadROMFromPath(handle, rom_path);
}

bool GBA_LoadROMFromMemory(GBACoreHandle* handle, const void* rom_data, size_t rom_size) {
  return EmulatorCore_LoadROMFromMemory(handle, rom_data, rom_size);
}

void GBA_StepFrame(GBACoreHandle* handle) {
  EmulatorCore_StepFrame(handle);
}

void GBA_SetKeyStatus(GBACoreHandle* handle, GBAKey key, bool pressed) {
  EmulatorCore_SetKeyStatus(handle, (EmulatorKey)key, pressed);
}

const uint32_t* GBA_GetFrameBufferRGBA(GBACoreHandle* handle, size_t* pixel_count) {
  return EmulatorCore_GetFrameBufferRGBA(handle, pixel_count);
}

const char* GBA_GetLastError(GBACoreHandle* handle) {
  return EmulatorCore_GetLastError(handle);
}

}  // extern "C"
