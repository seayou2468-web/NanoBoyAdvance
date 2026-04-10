#include "../../core_adapter.hpp"

#include <memory>

#include "runtime.hpp"

namespace {

void* CreateRuntime() {
  return core::quick_nes::CreateRuntime().release();
}

void DestroyRuntime(void* runtime) {
  delete static_cast<core::quick_nes::Runtime*>(runtime);
}

bool LoadBIOSFromPath(void*, const char*, std::string& last_error) {
  last_error = "NES core does not use external BIOS";
  return false;
}

bool LoadROMFromPath(void* runtime, const char* rom_path, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::quick_nes::LoadROMFromPath(*static_cast<core::quick_nes::Runtime*>(runtime), rom_path, last_error);
}

void StepFrame(void* runtime, std::string& last_error) {
  if (runtime == nullptr) {
    return;
  }
  core::quick_nes::StepFrame(*static_cast<core::quick_nes::Runtime*>(runtime), last_error);
}

void SetKeyStatus(void* runtime, int key, bool pressed) {
  if (runtime == nullptr) {
    return;
  }
  core::quick_nes::SetKeyStatus(*static_cast<core::quick_nes::Runtime*>(runtime), key, pressed);
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (out_spec == nullptr) {
    return false;
  }
  out_spec->width = 256;
  out_spec->height = 240;
  return true;
}

const uint32_t* GetFrameBufferRGBA(void* runtime, size_t* pixel_count) {
  if (runtime == nullptr) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }
  return core::quick_nes::GetFrameBufferRGBA(*static_cast<core::quick_nes::Runtime*>(runtime), pixel_count);
}

}  // namespace

namespace core {

const CoreAdapter kQuickNesAdapter = {
  .name = "quick_nes",
  .type = EMULATOR_CORE_TYPE_NES,
  .create_runtime = CreateRuntime,
  .destroy_runtime = DestroyRuntime,
  .load_bios_from_path = LoadBIOSFromPath,
  .load_rom_from_path = LoadROMFromPath,
  .step_frame = StepFrame,
  .set_key_status = SetKeyStatus,
  .get_video_spec = GetVideoSpec,
  .get_framebuffer_rgba = GetFrameBufferRGBA,
};

}  // namespace core
