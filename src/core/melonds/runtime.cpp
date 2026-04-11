#include "runtime.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

#include "GPU.h"
#include "NDS.h"
#include "Platform.h"

namespace core::melonds {
namespace {

constexpr size_t kFramePixels = 256U * 384U;
constexpr uint32_t kTopScreenHeight = 192U;
constexpr uint32_t kFrameWidth = 256U;
constexpr uint32_t kFrameHeight = 384U;

std::mutex g_core_lock;
bool g_in_use = false;
bool g_core_initialized = false;

bool ReadBinaryFile(const char* path, std::vector<uint8_t>& out, std::string& last_error) {
  if (path == nullptr || path[0] == '\0') {
    last_error = "file path is empty";
    return false;
  }
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    last_error = "failed to open file";
    return false;
  }
  const std::streamsize size = stream.tellg();
  if (size <= 0) {
    last_error = "file is empty";
    return false;
  }
  stream.seekg(0, std::ios::beg);
  out.resize(static_cast<size_t>(size));
  if (!stream.read(reinterpret_cast<char*>(out.data()), size)) {
    last_error = "failed to read file";
    return false;
  }
  return true;
}

void ClearFrame(Runtime& runtime) {
  std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0xFF000000U);
}

bool CopyFramebuffer(Runtime& runtime, std::string& last_error) {
  const int front = GPU::FrontBuffer;
  if (front < 0 || front > 1) {
    last_error = "invalid GPU front buffer index";
    return false;
  }

  const int candidates[2] = {front, front ? 0 : 1};
  for (int idx = 0; idx < 2; ++idx) {
    const int buffer_index = candidates[idx];
    const uint32_t* top = GPU::Framebuffer[buffer_index][0];
    const uint32_t* bottom = GPU::Framebuffer[buffer_index][1];
    if (top == nullptr && bottom == nullptr) {
      continue;
    }

    uint32_t* out_top = runtime.frame_rgba.data();
    uint32_t* out_bottom = runtime.frame_rgba.data() + (kFrameWidth * kTopScreenHeight);
    if (top != nullptr) {
      std::memcpy(out_top, top, sizeof(uint32_t) * kFrameWidth * kTopScreenHeight);
    } else {
      std::fill_n(out_top, kFrameWidth * kTopScreenHeight, 0xFF000000U);
    }
    if (bottom != nullptr) {
      std::memcpy(out_bottom, bottom, sizeof(uint32_t) * kFrameWidth * kTopScreenHeight);
    } else {
      std::fill_n(out_bottom, kFrameWidth * kTopScreenHeight, 0xFF000000U);
    }
    return true;
  }

  last_error = "GPU framebuffer is not available";
  return false;
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_in_use) {
    return nullptr;
  }

  if (!g_core_initialized) {
    Platform::Init(0, nullptr);
    Platform::SetConfigBool(Platform::ExternalBIOSEnable, true);
    Platform::SetConfigInt(Platform::AudioBitrate, 2);

    NDS::SetConsoleType(0);
    if (!NDS::Init()) {
      Platform::DeInit();
      return nullptr;
    }
    GPU::InitRenderer(0);
    GPU::RenderSettings render_settings{};
    render_settings.Soft_Threaded = false;
    render_settings.GL_ScaleFactor = 1;
    render_settings.GL_BetterPolygons = false;
    GPU::SetRenderSettings(0, render_settings);
    g_core_initialized = true;
  }

  auto runtime = std::make_unique<Runtime>();
  runtime->initialized = true;
  ClearFrame(*runtime);
  g_in_use = true;
  return runtime;
}

bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error) {
  if (!runtime.initialized) {
    last_error = "melonDS runtime is not initialized";
    return false;
  }

  std::vector<uint8_t> data;
  if (!ReadBinaryFile(bios_path, data, last_error)) {
    return false;
  }
  const std::string resolved_path = std::filesystem::absolute(std::filesystem::path(bios_path)).string();

  if (data.size() == 0x1000U) {
    runtime.bios9_data = std::move(data);
    runtime.bios9_path = resolved_path;
    Platform::SetConfigString(Platform::BIOS9Path, runtime.bios9_path);
    return true;
  }
  if (data.size() == 0x4000U) {
    runtime.bios7_data = std::move(data);
    runtime.bios7_path = resolved_path;
    Platform::SetConfigString(Platform::BIOS7Path, runtime.bios7_path);
    return true;
  }
  if (data.size() == 0x20000U || data.size() == 0x40000U || data.size() == 0x80000U) {
    runtime.firmware_data = std::move(data);
    runtime.firmware_path = resolved_path;
    Platform::SetConfigString(Platform::FirmwarePath, runtime.firmware_path);
    return true;
  }

  last_error = "unsupported BIOS/firmware size (expected 4KB ARM9, 16KB ARM7, or 128/256/512KB firmware)";
  return false;
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  std::vector<uint8_t> rom_data;
  if (!ReadBinaryFile(rom_path, rom_data, last_error)) {
    return false;
  }
  return LoadROMFromMemory(runtime, rom_data.data(), rom_data.size(), last_error);
}

bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  if (!runtime.initialized) {
    last_error = "melonDS runtime is not initialized";
    return false;
  }
  if (rom_data == nullptr || rom_size == 0 || rom_size > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    last_error = "invalid ROM image";
    return false;
  }

  runtime.rom_data.assign(static_cast<const uint8_t*>(rom_data), static_cast<const uint8_t*>(rom_data) + rom_size);
  NDS::LoadBIOS();
  runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);
  if (!runtime.rom_loaded) {
    last_error = "melonDS failed to load NDS cart";
    return false;
  }
  if (NDS::NeedsDirectBoot()) {
    NDS::SetupDirectBoot("game.nds");
  }
  NDS::Start();
  last_error.clear();
  return true;
}

void StepFrame(Runtime& runtime, std::string& last_error) {
  if (!runtime.rom_loaded) {
    last_error = "melonDS ROM is not loaded";
    return;
  }
  uint32_t key_mask = 0x000003FFU;
  auto set_pressed = [&](size_t idx, uint32_t bit) {
    if (idx < runtime.key_state.size() && runtime.key_state[idx]) {
      key_mask &= ~bit;
    }
  };
  set_pressed(0, 1U << 0U);  // A
  set_pressed(1, 1U << 1U);  // B
  set_pressed(2, 1U << 3U);  // Start
  set_pressed(3, 1U << 2U);  // Select
  set_pressed(4, 1U << 6U);  // Up
  set_pressed(5, 1U << 7U);  // Down
  set_pressed(6, 1U << 5U);  // Left
  set_pressed(7, 1U << 4U);  // Right
  set_pressed(8, 1U << 9U);  // L
  set_pressed(9, 1U << 8U);  // R
  NDS::SetKeyMask(key_mask);
  NDS::RunFrame();
  if (!CopyFramebuffer(runtime, last_error)) {
    return;
  }
  runtime.frame_counter++;
  last_error.clear();
}

void SetKeyStatus(Runtime& runtime, int key, bool pressed) {
  if (key < 0 || static_cast<size_t>(key) >= runtime.key_state.size()) {
    return;
  }
  runtime.key_state[static_cast<size_t>(key)] = pressed;
}

const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count) {
  if (pixel_count != nullptr) {
    *pixel_count = kFramePixels;
  }
  return runtime.frame_rgba.data();
}

bool SaveStateToBuffer(Runtime&, void*, size_t, size_t*, std::string& last_error) {
  last_error = "melonDS state buffer API is not implemented yet";
  return false;
}

bool LoadStateFromBuffer(Runtime&, const void*, size_t, std::string& last_error) {
  last_error = "melonDS state buffer API is not implemented yet";
  return false;
}

bool ApplyCheatCode(Runtime&, const char*, std::string& last_error) {
  last_error = "melonDS cheat API is not implemented yet";
  return false;
}

void ReleaseRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_core_initialized) {
    NDS::Stop();
    NDS::DeInit();
    Platform::DeInit();
    g_core_initialized = false;
  }
  g_in_use = false;
}

}  // namespace core::melonds
