#include "../core_adapter.hpp"

#include <memory>

#include "runtime.hpp"

namespace {

void* CreateRuntime() {
  return core::gba::CreateRuntime().release();
}

void DestroyRuntime(void* runtime) {
  delete static_cast<core::gba::Runtime*>(runtime);
}

bool LoadBIOSFromPath(void* runtime, const char* bios_path, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::LoadBIOSFromPath(*static_cast<core::gba::Runtime*>(runtime), bios_path, last_error);
}

bool LoadROMFromPath(void* runtime, const char* rom_path, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::LoadROMFromPath(*static_cast<core::gba::Runtime*>(runtime), rom_path, last_error);
}

bool LoadROMFromMemory(void* runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::LoadROMFromMemory(*static_cast<core::gba::Runtime*>(runtime), rom_data, rom_size, last_error);
}

void StepFrame(void* runtime, std::string& last_error) {
  if (runtime == nullptr) {
    return;
  }
  core::gba::StepFrame(*static_cast<core::gba::Runtime*>(runtime), last_error);
}

void SetKeyStatus(void* runtime, int key, bool pressed) {
  if (runtime == nullptr) {
    return;
  }
  core::gba::SetKeyStatus(*static_cast<core::gba::Runtime*>(runtime), key, pressed);
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (out_spec == nullptr) {
    return false;
  }
  out_spec->width = 240;
  out_spec->height = 160;
  out_spec->pixel_format = EMULATOR_PIXEL_FORMAT_RGBA8888;
  return true;
}

const uint32_t* GetFrameBufferRGBA(void* runtime, size_t* pixel_count) {
  if (runtime == nullptr) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }
  return core::gba::GetFrameBufferRGBA(*static_cast<core::gba::Runtime*>(runtime), pixel_count);
}

bool SaveStateToBuffer(void* runtime, void* out_buffer, size_t buffer_size, size_t* out_size, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::SaveStateToBuffer(
      *static_cast<core::gba::Runtime*>(runtime), out_buffer, buffer_size, out_size, last_error);
}

bool LoadStateFromBuffer(void* runtime, const void* state_buffer, size_t state_size, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::LoadStateFromBuffer(
      *static_cast<core::gba::Runtime*>(runtime), state_buffer, state_size, last_error);
}

bool ApplyCheatCode(void* runtime, const char* cheat_code, std::string& last_error) {
  if (runtime == nullptr) {
    last_error = "core runtime is not initialized";
    return false;
  }
  return core::gba::ApplyCheatCode(
      *static_cast<core::gba::Runtime*>(runtime), cheat_code, last_error);
}

}  // namespace

namespace core {

extern const CoreAdapter kGBAAdapter = {
  .name = "nanoboyadvance",
  .type = EMULATOR_CORE_TYPE_GBA,
  .create_runtime = CreateRuntime,
  .destroy_runtime = DestroyRuntime,
  .load_bios_from_path = LoadBIOSFromPath,
  .load_rom_from_path = LoadROMFromPath,
  .load_rom_from_memory = LoadROMFromMemory,
  .step_frame = StepFrame,
  .set_key_status = SetKeyStatus,
  .get_video_spec = GetVideoSpec,
  .get_framebuffer_rgba = GetFrameBufferRGBA,
  .save_state_to_buffer = SaveStateToBuffer,
  .load_state_from_buffer = LoadStateFromBuffer,
  .apply_cheat_code = ApplyCheatCode,
};

}  // namespace core
