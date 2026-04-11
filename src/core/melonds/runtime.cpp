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

std::mutex g_core_lock;
bool g_in_use = false;

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

u32 BuildKeyMask(const std::array<bool, 10>& key_state) {
  u32 mask = 0x3FF;
  if (key_state[0]) mask &= ~(1U << 0);   // A
  if (key_state[1]) mask &= ~(1U << 1);   // B
  if (key_state[2]) mask &= ~(1U << 2);   // Select
  if (key_state[3]) mask &= ~(1U << 3);   // Start
  if (key_state[4]) mask &= ~(1U << 4);   // Right
  if (key_state[5]) mask &= ~(1U << 5);   // Left
  if (key_state[6]) mask &= ~(1U << 6);   // Up
  if (key_state[7]) mask &= ~(1U << 7);   // Down
  if (key_state[8]) mask &= ~(1U << 8);   // R
  if (key_state[9]) mask &= ~(1U << 9);   // L
  return mask;
}

void BlitFrame(Runtime& runtime) {
  const int front = GPU::FrontBuffer;
  const u32* top = GPU::Framebuffer[front][0];
  const u32* bottom = GPU::Framebuffer[front][1];
  if (top == nullptr || bottom == nullptr) {
    return;
  }

  for (size_t y = 0; y < 192U; ++y) {
    std::memcpy(runtime.frame_rgba.data() + y * 256U, top + y * 256U, sizeof(u32) * 256U);
    std::memcpy(runtime.frame_rgba.data() + (y + 192U) * 256U, bottom + y * 256U, sizeof(u32) * 256U);
  }
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_in_use) {
    return nullptr;
  }

  auto runtime = std::make_unique<Runtime>();

  Platform::SetConfigBool(Platform::ExternalBIOSEnable, true);
  Platform::SetConfigBool(Platform::DLDI_Enable, false);
  Platform::SetConfigBool(Platform::DSiSD_Enable, false);
  Platform::SetConfigInt(Platform::AudioBitrate, 2);

  if (!NDS::Init()) {
    return nullptr;
  }

  NDS::SetConsoleType(0);
  NDS::Reset();
  g_in_use = true;
  runtime->initialized = true;
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

  // Auto-detect ARM9/ARM7 BIOS by size.
  if (data.size() == 0x1000U) {
    runtime.bios9_data = std::move(data);
    Platform::SetConfigString(Platform::BIOS9Path, bios_path);
    return true;
  }
  if (data.size() == 0x4000U) {
    runtime.bios7_data = std::move(data);
    Platform::SetConfigString(Platform::BIOS7Path, bios_path);
    return true;
  }

  last_error = "unsupported BIOS size (expected 4KB ARM9 or 16KB ARM7)";
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
  if (rom_data == nullptr || rom_size == 0 || rom_size > static_cast<size_t>(std::numeric_limits<u32>::max())) {
    last_error = "invalid ROM image";
    return false;
  }

  runtime.rom_data.assign(static_cast<const uint8_t*>(rom_data), static_cast<const uint8_t*>(rom_data) + rom_size);
  NDS::Reset();

  if (!NDS::LoadCart(runtime.rom_data.data(), static_cast<u32>(runtime.rom_data.size()), nullptr, 0)) {
    last_error = "failed to load NDS cartridge image";
    return false;
  }

  if (NDS::NeedsDirectBoot()) {
    NDS::SetupDirectBoot("game.nds");
  }
  NDS::Start();
  runtime.rom_loaded = true;
  return true;
}

void StepFrame(Runtime& runtime, std::string& last_error) {
  if (!runtime.rom_loaded) {
    return;
  }
  NDS::SetKeyMask(BuildKeyMask(runtime.key_state));
  NDS::RunFrame();
  BlitFrame(runtime);
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

}  // namespace core::melonds
