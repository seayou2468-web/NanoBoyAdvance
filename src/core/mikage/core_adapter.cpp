#if !defined(NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION)
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 0
#endif

#if NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION

#include "../core_adapter.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "./common/log_manager.h"
#include "./citra/emu_window/emu_window_ios.h"
#include "./core/loader.h"
#include "./core/system.h"
#include "./video_core/video_core.h"
#include "./video_core/renderer_software/renderer_software.h"

namespace {

struct MikageRuntime {
  EmuWindow_IOS window;
  bool initialized = false;
  bool rom_loaded = false;
  std::vector<std::string> bios_paths;
  std::vector<uint32_t> rgba_frame;
};

void* CreateRuntime() {
  auto* runtime = new MikageRuntime();
  runtime->rgba_frame.resize(static_cast<size_t>(VideoCore::kScreenTopWidth) *
                             static_cast<size_t>(VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight));
  return runtime;
}

void DestroyRuntime(void* opaque_runtime) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    return;
  }
  if (runtime->initialized) {
    System::Shutdown();
    LogManager::Shutdown();
  }
  delete runtime;
}

bool LoadBiosFromPath(void* opaque_runtime, const char* bios_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !bios_path || bios_path[0] == '\0') {
    last_error = "Invalid 3DS BIOS path";
    return false;
  }
  runtime->bios_paths.emplace_back(bios_path);
  return true;
}

bool EnsureInitialized(MikageRuntime* runtime, std::string& last_error) {
  if (runtime->initialized) return true;
  try {
    LogManager::Init();
    System::Init(&runtime->window);
    runtime->initialized = true;
    return true;
  } catch (...) {
    last_error = "Failed to initialize Mikage core";
    return false;
  }
}

bool LoadRomFromPath(void* opaque_runtime, const char* rom_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !rom_path || rom_path[0] == '\0') {
    last_error = "Invalid 3DS ROM path";
    return false;
  }

  if (!EnsureInitialized(runtime, last_error)) {
    return false;
  }

  try {
    std::string path = rom_path;
    if (!Loader::LoadFile(path, &last_error)) {
      if (last_error.empty()) {
        last_error = "Failed to load 3DS ROM";
      }
      runtime->rom_loaded = false;
      return false;
    }
  } catch (const std::exception& ex) {
    last_error = ex.what();
    runtime->rom_loaded = false;
    return false;
  } catch (...) {
    last_error = "Unexpected exception while loading 3DS ROM";
    runtime->rom_loaded = false;
    return false;
  }

  runtime->rom_loaded = true;
  return true;
}

bool LoadRomFromMemory(void*, const void*, size_t, std::string& last_error) {
  last_error = "Mikage 3DS core does not support in-memory ROM loading";
  return false;
}

void StepFrame(void* opaque_runtime, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->initialized || !runtime->rom_loaded) {
    return;
  }

  try {
    System::RunLoopFor(20000);

    auto* software_renderer = dynamic_cast<RendererSoftware*>(VideoCore::g_renderer);
    if (!software_renderer) {
      // Keep the ROM running even if a non-software renderer is active.
      return;
    }

    const auto& framebuffer = software_renderer->Framebuffer();
    const size_t copy_bytes = std::min(framebuffer.size(), runtime->rgba_frame.size() * sizeof(uint32_t));
    std::memcpy(runtime->rgba_frame.data(), framebuffer.data(), copy_bytes);
  } catch (const std::exception& ex) {
    last_error = ex.what();
    runtime->rom_loaded = false;
  } catch (...) {
    last_error = "Unexpected exception while stepping 3DS frame";
    runtime->rom_loaded = false;
  }
}

void SetKeyStatus(void*, int, bool) {
  // TODO: map iOS virtual/controller keys to Mikage input when input backend is wired.
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (!out_spec) {
    return false;
  }
  out_spec->width = static_cast<uint32_t>(VideoCore::kScreenTopWidth);
  out_spec->height = static_cast<uint32_t>(VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
  out_spec->pixel_format = EMULATOR_PIXEL_FORMAT_RGBA8888;
  return true;
}

const uint32_t* GetFrameBufferRGBA(void* opaque_runtime, size_t* pixel_count) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    if (pixel_count) *pixel_count = 0;
    return nullptr;
  }
  if (pixel_count) {
    *pixel_count = runtime->rgba_frame.size();
  }
  return runtime->rgba_frame.data();
}

bool SaveStateToBuffer(void*, void*, size_t, size_t*, std::string& last_error) {
  last_error = "Mikage savestates are not yet connected";
  return false;
}

bool LoadStateFromBuffer(void*, const void*, size_t, std::string& last_error) {
  last_error = "Mikage savestates are not yet connected";
  return false;
}

bool ApplyCheatCode(void*, const char*, std::string& last_error) {
  last_error = "Mikage cheats are not yet connected";
  return false;
}

}  // namespace

namespace core {

extern const CoreAdapter kMikageAdapter = {
    .name = "mikage",
    .type = EMULATOR_CORE_TYPE_3DS,
    .create_runtime = &CreateRuntime,
    .destroy_runtime = &DestroyRuntime,
    .load_bios_from_path = &LoadBiosFromPath,
    .load_rom_from_path = &LoadRomFromPath,
    .load_rom_from_memory = &LoadRomFromMemory,
    .step_frame = &StepFrame,
    .set_key_status = &SetKeyStatus,
    .get_video_spec = &GetVideoSpec,
    .get_framebuffer_rgba = &GetFrameBufferRGBA,
    .save_state_to_buffer = &SaveStateToBuffer,
    .load_state_from_buffer = &LoadStateFromBuffer,
    .apply_cheat_code = &ApplyCheatCode,
};

}  // namespace core

#endif  // NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION
