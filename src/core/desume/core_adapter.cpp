#include "../core_adapter.hpp"

#include "runtime.hpp"

#include "NDSSystem.h"

namespace {

void* CreateRuntime() {
  return core::desume::CreateRuntime().release();
}

void DestroyRuntime(void* runtime) {
  auto* desume_runtime = static_cast<core::desume::Runtime*>(runtime);
  if (desume_runtime != nullptr && desume_runtime->initialized) {
    NDS_DeInit();
  }
  delete desume_runtime;
}

bool LoadBIOSFromPath(void* runtime, const char* bios_path, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::desume::LoadBIOSFromPath(*static_cast<core::desume::Runtime*>(runtime), bios_path, last_error);
}

bool LoadROMFromPath(void* runtime, const char* rom_path, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::desume::LoadROMFromPath(*static_cast<core::desume::Runtime*>(runtime), rom_path, last_error);
}

bool LoadROMFromMemory(void* runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::desume::LoadROMFromMemory(*static_cast<core::desume::Runtime*>(runtime), rom_data, rom_size, last_error);
}

void StepFrame(void* runtime, std::string& last_error) {
  if (runtime == nullptr) {
    return;
  }
  core::desume::StepFrame(*static_cast<core::desume::Runtime*>(runtime), last_error);
}

void SetKeyStatus(void* runtime, int key, bool pressed) {
  if (runtime == nullptr) {
    return;
  }
  core::desume::SetKeyStatus(*static_cast<core::desume::Runtime*>(runtime), key, pressed);
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (out_spec == nullptr) {
    return false;
  }
  out_spec->width = 256;
  out_spec->height = 384;
  out_spec->pixel_format = EMULATOR_PIXEL_FORMAT_ARGB8888;
  return true;
}

const uint32_t* GetFrameBufferRGBA(void* runtime, size_t* pixel_count) {
  if (runtime == nullptr) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }
  return core::desume::GetFrameBufferRGBA(*static_cast<core::desume::Runtime*>(runtime), pixel_count);
}

}  // namespace

namespace core {

extern const CoreAdapter kDesumeAdapter = {
  .name = "desume",
  .type = EMULATOR_CORE_TYPE_NDS,
  .create_runtime = CreateRuntime,
  .destroy_runtime = DestroyRuntime,
  .load_bios_from_path = LoadBIOSFromPath,
  .load_rom_from_path = LoadROMFromPath,
  .load_rom_from_memory = LoadROMFromMemory,
  .step_frame = StepFrame,
  .set_key_status = SetKeyStatus,
  .get_video_spec = GetVideoSpec,
  .get_framebuffer_rgba = GetFrameBufferRGBA,
};

}  // namespace core
