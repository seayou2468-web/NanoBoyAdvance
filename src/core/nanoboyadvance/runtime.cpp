#include "runtime.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "nba/include/nba/config.hpp"
#include "nba/include/nba/core.hpp"
#include "nba/include/nba/device/video_device.hpp"
#include "nba/include/nba/integer.hpp"
#include "nba/include/nba/rom/rom.hpp"
#include "nba/include/nba/save_state.hpp"

namespace core::gba {

class CopyVideoDevice final : public nba::VideoDevice {
public:
  void Draw(u32* buffer) override {
    if (buffer == nullptr) {
      std::fill(frame_.begin(), frame_.end(), 0u);
      return;
    }
    std::copy_n(buffer, frame_.size(), frame_.begin());
  }

  const uint32_t* Data() const {
    return frame_.data();
  }

private:
  std::array<uint32_t, 240U * 160U> frame_{};
};

namespace {

constexpr size_t kFramePixels = 240U * 160U;
constexpr size_t kMaxRomBytes = 32U * 1024U * 1024U;
constexpr size_t kMaxBiosBytes = 64U * 1024U;

bool ReadBinaryFile(const std::filesystem::path& path, std::vector<u8>& out, size_t min_size, size_t max_size) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
    return false;
  }

  std::error_code ec;
  auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size < min_size || file_size > max_size) {
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  out.resize(static_cast<size_t>(file_size));
  file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(file_size));
  return file.good() || file.eof();
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  auto runtime = std::make_unique<Runtime>();
  runtime->video_device = std::make_shared<CopyVideoDevice>();
  runtime->config = std::make_shared<nba::Config>();
  runtime->config->skip_bios = true;
  runtime->config->video_dev = runtime->video_device;
  runtime->core = nba::CreateCore(runtime->config);
  return runtime;
}

bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error) {
  if (bios_path == nullptr || bios_path[0] == '\0') {
    return false;
  }

  std::vector<u8> bios;
  if (!ReadBinaryFile(std::filesystem::path(bios_path), bios, 16U * 1024U, kMaxBiosBytes)) {
    last_error = "failed to read BIOS file";
    return false;
  }

  runtime.bios_data = std::move(bios);
  runtime.config->skip_bios = false;
  return true;
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  if (rom_path == nullptr || rom_path[0] == '\0') {
    return false;
  }

  std::vector<u8> rom_data;
  if (!ReadBinaryFile(std::filesystem::path(rom_path), rom_data, 192U, kMaxRomBytes)) {
    last_error = "failed to read ROM file";
    return false;
  }

  return LoadROMFromMemory(runtime, rom_data.data(), rom_data.size(), last_error);
}

bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  if (rom_data == nullptr || rom_size < 192U || rom_size > kMaxRomBytes) {
    last_error = "invalid ROM image";
    return false;
  }

  std::vector<u8> rom_bytes(static_cast<const u8*>(rom_data), static_cast<const u8*>(rom_data) + rom_size);

  try {
    runtime.config->skip_bios = runtime.bios_data.empty();
    runtime.core = nba::CreateCore(runtime.config);
    if (!runtime.bios_data.empty()) {
      runtime.core->Attach(runtime.bios_data);
    }
    runtime.core->Attach(nba::ROM{std::move(rom_bytes), std::unique_ptr<nba::Backup>{}, std::unique_ptr<nba::GPIO>{}});
    runtime.core->Reset();
    return true;
  } catch (const std::exception& ex) {
    last_error = ex.what();
  } catch (...) {
    last_error = "unknown exception while loading ROM";
  }

  return false;
}

void StepFrame(Runtime& runtime, std::string& last_error) {
  if (!runtime.core) {
    return;
  }

  try {
    runtime.core->RunForOneFrame();
  } catch (const std::exception& ex) {
    last_error = ex.what();
  } catch (...) {
    last_error = "unknown exception while stepping frame";
  }
}

void SetKeyStatus(Runtime& runtime, int key, bool pressed) {
  if (!runtime.core) {
    return;
  }

  runtime.core->SetKeyStatus(static_cast<nba::Key>(key), pressed);
}

const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count) {
  if (pixel_count != nullptr) {
    *pixel_count = kFramePixels;
  }
  if (!runtime.video_device) {
    return nullptr;
  }
  return runtime.video_device->Data();
}

bool SaveStateToBuffer(Runtime& runtime, void* out_buffer, size_t buffer_size, size_t* out_size, std::string& last_error) {
  if (!runtime.core) {
    last_error = "core is not initialized";
    return false;
  }

  constexpr size_t kStateSize = sizeof(nba::SaveState);
  if (out_size != nullptr) {
    *out_size = kStateSize;
  }
  if (out_buffer == nullptr) {
    return true;
  }
  if (buffer_size < kStateSize) {
    last_error = "state buffer is too small";
    return false;
  }

  nba::SaveState state{};
  runtime.core->CopyState(state);
  std::memcpy(out_buffer, &state, kStateSize);
  return true;
}

bool LoadStateFromBuffer(Runtime& runtime, const void* state_buffer, size_t state_size, std::string& last_error) {
  if (!runtime.core) {
    last_error = "core is not initialized";
    return false;
  }
  if (state_buffer == nullptr || state_size != sizeof(nba::SaveState)) {
    last_error = "invalid save-state buffer";
    return false;
  }

  nba::SaveState state{};
  std::memcpy(&state, state_buffer, sizeof(state));
  runtime.core->LoadState(state);
  return true;
}

bool ApplyCheatCode(Runtime&, const char*, std::string& last_error) {
  last_error = "cheat is not implemented for GBA core yet";
  return false;
}

}  // namespace core::gba
