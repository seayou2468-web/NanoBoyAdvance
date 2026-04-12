#include "runtime.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>

namespace core::sameboy {

namespace {
constexpr size_t kFramePixels = 160U * 144U;
constexpr size_t kMaxRomBytes = 8U * 1024U * 1024U;
constexpr size_t kMaxBiosBytes = 16U * 1024U;

SBA_Key ToSameBoyKey(int key) {
  switch (key) {
    case 0: return SBA_KEY_A;
    case 1: return SBA_KEY_B;
    case 2: return SBA_KEY_SELECT;
    case 3: return SBA_KEY_START;
    case 4: return SBA_KEY_RIGHT;
    case 5: return SBA_KEY_LEFT;
    case 6: return SBA_KEY_UP;
    case 7: return SBA_KEY_DOWN;
    default: return static_cast<SBA_Key>(-1);
  }
}

void ResetCore(Runtime& runtime) {
  if (runtime.gb != nullptr) SBA_destroy(runtime.gb);
  runtime.gb = SBA_create(SBA_MODEL_CGB_E);
  runtime.key_state.fill(false);
  std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0U);
  if (runtime.gb != nullptr) {
    SBA_use_default_argb_encoder(runtime.gb);
    SBA_set_pixels_output(runtime.gb, runtime.frame_rgba.data());
    if (!runtime.gbc_bios_storage.empty()) SBA_load_gbc_boot_rom_from_buffer(runtime.gb, runtime.gbc_bios_storage.data(), runtime.gbc_bios_storage.size());
    if (!runtime.bios_storage.empty()) SBA_load_boot_rom_from_buffer(runtime.gb, runtime.bios_storage.data(), runtime.bios_storage.size());
  }
}

bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& out, size_t min_size, size_t max_size) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) return false;
  std::error_code ec;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size < min_size || file_size > max_size) return false;
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return false;
  out.resize(static_cast<size_t>(file_size));
  file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(file_size));
  return file.good() || file.eof();
}
}

std::unique_ptr<Runtime> CreateRuntime() {
  auto runtime = std::make_unique<Runtime>();
  ResetCore(*runtime);
  return runtime;
}

bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error) {
  try {
    if (bios_path == nullptr || bios_path[0] == '\0') return false;
    std::vector<uint8_t> bios;
    if (!ReadBinaryFile(std::filesystem::path(bios_path), bios, 256U, kMaxBiosBytes)) return false;
    std::string path(bios_path);
    if (path.find("gbc") != std::string::npos || bios.size() == 2304) runtime.gbc_bios_storage = std::move(bios);
    else runtime.bios_storage = std::move(bios);
    if (runtime.gb != nullptr) {
      if (!runtime.gbc_bios_storage.empty()) SBA_load_gbc_boot_rom_from_buffer(runtime.gb, runtime.gbc_bios_storage.data(), runtime.gbc_bios_storage.size());
      if (!runtime.bios_storage.empty()) SBA_load_boot_rom_from_buffer(runtime.gb, runtime.bios_storage.data(), runtime.bios_storage.size());
    }
    return true;
  } catch (...) { return false; }
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  try {
    if (rom_path == nullptr || rom_path[0] == '\0') return false;
    ResetCore(runtime);
    if (runtime.gb == nullptr) return false;
    std::vector<uint8_t> rom_data;
    if (!ReadBinaryFile(std::filesystem::path(rom_path), rom_data, 192U, kMaxRomBytes)) return false;
    runtime.rom_storage = std::move(rom_data);
    SBA_load_rom_from_buffer(runtime.gb, runtime.rom_storage.data(), runtime.rom_storage.size());
    return true;
  } catch (...) { return false; }
}

bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  try {
    if (rom_data == nullptr || rom_size == 0) return false;
    ResetCore(runtime);
    if (runtime.gb == nullptr) return false;
    runtime.rom_storage.resize(rom_size);
    std::memcpy(runtime.rom_storage.data(), rom_data, rom_size);
    SBA_load_rom_from_buffer(runtime.gb, runtime.rom_storage.data(), runtime.rom_storage.size());
    return true;
  } catch (...) { return false; }
}

void StepFrame(Runtime& runtime, std::string&) { if (runtime.gb) SBA_run_frame(runtime.gb); }
void SetKeyStatus(Runtime& runtime, int key, bool pressed) {
  if (runtime.gb == nullptr || key < 0 || (size_t)key >= runtime.key_state.size()) return;
  runtime.key_state[(size_t)key] = pressed;
  const SBA_Key gb_key = ToSameBoyKey(key);
  if ((int)gb_key >= 0) SBA_set_key_state(runtime.gb, gb_key, pressed);
}
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count) {
  if (pixel_count) *pixel_count = kFramePixels;
  return runtime.frame_rgba.data();
}
bool SaveStateToBuffer(Runtime&, void*, size_t, size_t*, std::string&) { return false; }
bool LoadStateFromBuffer(Runtime&, const void*, size_t, std::string&) { return false; }
bool ApplyCheatCode(Runtime&, const char*, std::string&) { return false; }
}
