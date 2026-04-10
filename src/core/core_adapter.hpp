#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "api/emulator_core_c_api.h"

namespace core {

struct CoreAdapter {
  const char* name;
  EmulatorCoreType type;

  void* (*create_runtime)();
  void (*destroy_runtime)(void* runtime);

  bool (*load_bios_from_path)(void* runtime, const char* bios_path, std::string& last_error);
  bool (*load_rom_from_path)(void* runtime, const char* rom_path, std::string& last_error);
  void (*step_frame)(void* runtime, std::string& last_error);
  void (*set_key_status)(void* runtime, int key, bool pressed);
  bool (*get_video_spec)(EmulatorVideoSpec* out_spec);
  const uint32_t* (*get_framebuffer_rgba)(void* runtime, size_t* pixel_count);
};

const CoreAdapter* FindCoreAdapter(EmulatorCoreType type);

}  // namespace core
