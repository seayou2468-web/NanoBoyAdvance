#if !defined(NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION)
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 0
#endif

#if NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION

#include "../core_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "./include/emulator.hpp"
#include "./include/jscore_runtime.hpp"
#include "./include/services/hid.hpp"

namespace {

constexpr uint32_t kMikageFrameWidth = Emulator::width;
constexpr uint32_t kMikageFrameHeight = Emulator::height;

struct MikageRuntime {
  bool rom_loaded = false;
  std::vector<std::string> bios_paths;
  std::vector<std::string> pending_cheat_codes;
  std::vector<uint32_t> rgba_frame;
  std::filesystem::path temp_rom_path;
  std::unique_ptr<Emulator> emulator;
};

void* CreateRuntime() {
  auto* runtime = new MikageRuntime();
  runtime->rgba_frame.assign(static_cast<size_t>(kMikageFrameWidth) * static_cast<size_t>(kMikageFrameHeight),
                             0xFF000000u);

  try {
    runtime->emulator = std::make_unique<Emulator>();
  } catch (...) {
    delete runtime;
    return nullptr;
  }

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

  RemoveTemporaryRomIfAny(runtime);
  runtime->emulator.reset();
  delete runtime;
}

bool LoadBiosFromPath(void* opaque_runtime, const char* bios_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !bios_path || bios_path[0] == '\0') {
    last_error = "Invalid 3DS BIOS path";
    return false;
  }

  std::filesystem::path path(bios_path);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    last_error = "3DS BIOS path does not exist";
    return false;
  }

  runtime->bios_paths.emplace_back(bios_path);
  return true;
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

bool LoadRomFromPath(void* opaque_runtime, const char* rom_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator || !rom_path || rom_path[0] == '\0') {
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

  std::filesystem::path fs_path(path);
  std::error_code ec;
  if (!std::filesystem::exists(fs_path, ec) || !std::filesystem::is_regular_file(fs_path, ec)) {
    last_error = "3DS ROM path does not exist or is not a file";
    return false;
  }

  if (!runtime->emulator->loadROM(fs_path)) {
    last_error = "Mikage failed to load 3DS ROM";
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

  RemoveTemporaryRomIfAny(runtime);

  std::error_code ec;
  const auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    last_error = "Failed to get temporary directory for 3DS ROM staging";
    return false;
  }

  const auto staging_file = temp_dir / "mikage_memrom.3ds";
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

uint32_t CoreKeyToHidMask(int key) {
  switch (key) {
    case 0: return HID::Keys::A;
    case 1: return HID::Keys::B;
    case 2: return HID::Keys::Select;
    case 3: return HID::Keys::Start;
    case 4: return HID::Keys::Right;
    case 5: return HID::Keys::Left;
    case 6: return HID::Keys::Up;
    case 7: return HID::Keys::Down;
    case 8: return HID::Keys::R;
    case 9: return HID::Keys::L;
    default: return HID::Keys::Null;
  }
}

void StepFrame(void* opaque_runtime, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator || !runtime->rom_loaded) {
    return;
  }

  try {
    runtime->emulator->runFrame();
  } catch (...) {
    last_error = "Mikage frame execution failed";
  }
}

void SetKeyStatus(void* opaque_runtime, int key, bool pressed) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator) {
    return;
  }

  const uint32_t mask = CoreKeyToHidMask(key);
  if (mask == HID::Keys::Null) {
    return;
  }

  auto& hid = runtime->emulator->getServiceManager().getHID();
  hid.setKey(mask, pressed);
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (!out_spec) {
    return false;
  }
  out_spec->width = kMikageFrameWidth;
  out_spec->height = kMikageFrameHeight;
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

bool NormalizeCheatCodeWithJavaScriptCore(const char* cheat_code, std::string& normalized, std::string& last_error) {
  if (!cheat_code || cheat_code[0] == '\0') {
    last_error = "Cheat code is empty";
    return false;
  }

  normalized = cheat_code;
  constexpr const char* kJsPrefix = "js:";
  if (normalized.rfind(kJsPrefix, 0) != 0) {
    return true;
  }

  const std::string script = normalized.substr(std::strlen(kJsPrefix));
  std::string script_output;
  if (!jscore_runtime::EvaluateScriptToString(script, script_output, last_error)) {
    last_error = "JavaScriptCore evaluation failed: " + last_error;
    return false;
  }

  if (script_output.empty()) {
    last_error = "JavaScriptCore script returned an empty cheat code";
    return false;
  }

  normalized = script_output;
  return true;
}

bool ApplyCheatCode(void* opaque_runtime, const char* cheat_code, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    last_error = "Mikage runtime is not initialized";
    return false;
  }

  std::string normalized_cheat;
  if (!NormalizeCheatCodeWithJavaScriptCore(cheat_code, normalized_cheat, last_error)) {
    return false;
  }

  runtime->pending_cheat_codes.push_back(std::move(normalized_cheat));
  return true;
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
