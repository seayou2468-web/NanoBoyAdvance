#include "runtime.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

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

void ClearFrame(Runtime& runtime) {
  std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0xFF000000U);
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_in_use) {
    return nullptr;
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

  if (data.size() == 0x1000U) {
    runtime.bios9_data = std::move(data);
    return true;
  }
  if (data.size() == 0x4000U) {
    runtime.bios7_data = std::move(data);
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
  if (rom_data == nullptr || rom_size == 0 || rom_size > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    last_error = "invalid ROM image";
    return false;
  }

  runtime.rom_data.assign(static_cast<const uint8_t*>(rom_data), static_cast<const uint8_t*>(rom_data) + rom_size);
  runtime.rom_loaded = true;
  last_error.clear();
  return true;
}

void StepFrame(Runtime& runtime, std::string& last_error) {
  if (!runtime.rom_loaded) {
    last_error = "melonDS ROM is not loaded";
    return;
  }
  ClearFrame(runtime);
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
  g_in_use = false;
}

}  // namespace core::melonds
