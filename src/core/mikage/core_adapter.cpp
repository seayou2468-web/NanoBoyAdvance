#if !defined(NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION)
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 0
#endif

#if NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION

#include "../core_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
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
  std::filesystem::path temp_rom_path;
};

void* CreateRuntime() {
  auto* runtime = new MikageRuntime();
  runtime->rgba_frame.resize(static_cast<size_t>(VideoCore::kScreenTopWidth) *
                             static_cast<size_t>(VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight));
  return runtime;
}

void RemoveTemporaryRomIfAny(MikageRuntime* runtime) {
  if (!runtime || runtime->temp_rom_path.empty()) {
    return;
  }

  std::error_code ec;
  std::filesystem::remove(runtime->temp_rom_path, ec);
  runtime->temp_rom_path.clear();
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
  RemoveTemporaryRomIfAny(runtime);
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

int DecodeHexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;
}

std::string DecodeFileUrlPath(std::string path) {
  std::string decoded;
  decoded.reserve(path.size());
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '%' && (i + 2) < path.size()) {
      const int hi = DecodeHexNibble(path[i + 1]);
      const int lo = DecodeHexNibble(path[i + 2]);
      if (hi >= 0 && lo >= 0) {
        decoded.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    decoded.push_back(path[i]);
  }
  return decoded;
}

void ResetSystemForFreshBoot(MikageRuntime* runtime) {
  if (!runtime || !runtime->initialized) {
    return;
  }
  System::Shutdown();
  runtime->initialized = false;
  runtime->rom_loaded = false;
  RemoveTemporaryRomIfAny(runtime);
}

bool LoadRomFromPath(void* opaque_runtime, const char* rom_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !rom_path || rom_path[0] == '\0') {
    last_error = "Invalid 3DS ROM path";
    return false;
  }

  std::string path = rom_path;
  if (path.rfind("file://", 0) == 0) {
    path = path.substr(7);
    if (path.rfind("localhost/", 0) == 0) {
      path = path.substr(std::strlen("localhost"));
    }
    path = DecodeFileUrlPath(path);
  }

  const std::filesystem::path fs_path(path);
  std::error_code ec;
  if (!std::filesystem::exists(fs_path, ec) || !std::filesystem::is_regular_file(fs_path, ec)) {
    last_error = "3DS ROM path does not exist or is not a file";
    return false;
  }

  if (runtime->rom_loaded) {
    ResetSystemForFreshBoot(runtime);
  }

  if (!EnsureInitialized(runtime, last_error)) {
    return false;
  }

  try {
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

bool LoadRomFromMemory(void* opaque_runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    last_error = "Mikage runtime is not initialized";
    return false;
  }
  if (!rom_data || rom_size == 0) {
    last_error = "Invalid 3DS ROM memory buffer";
    return false;
  }

  if (runtime->rom_loaded) {
    ResetSystemForFreshBoot(runtime);
  } else {
    RemoveTemporaryRomIfAny(runtime);
  }

  std::error_code ec;
  const auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    last_error = "Failed to get temporary directory for 3DS ROM staging";
    return false;
  }

  std::mt19937_64 rng(std::random_device{}());
  std::filesystem::path staging_file;
  bool created = false;
  for (int attempt = 0; attempt < 32; ++attempt) {
    const uint64_t token = rng();
    const auto candidate = temp_dir / ("mikage_memrom_" + std::to_string(token) + ".3ds");
    if (std::filesystem::exists(candidate, ec)) {
      continue;
    }
    staging_file = candidate;
    created = true;
    break;
  }
  if (!created) {
    last_error = "Failed to allocate a unique temporary file path for 3DS ROM staging";
    return false;
  }

  std::ofstream out(staging_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    last_error = "Failed to create temporary file for 3DS ROM staging";
    return false;
  }
  out.write(static_cast<const char*>(rom_data), static_cast<std::streamsize>(rom_size));
  if (!out.good()) {
    out.close();
    std::filesystem::remove(staging_file, ec);
    last_error = "Failed to write 3DS ROM memory buffer to temporary file";
    return false;
  }
  out.close();

  std::string path = staging_file.string();
  if (!LoadRomFromPath(runtime, path.c_str(), last_error)) {
    std::filesystem::remove(staging_file, ec);
    return false;
  }

  runtime->temp_rom_path = staging_file;
  return true;
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
