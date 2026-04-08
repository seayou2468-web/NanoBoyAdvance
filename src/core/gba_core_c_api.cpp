#include "gba_core_c_api.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__linux__) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#error "gba_core_c_api supports only iOS and Linux targets"
#endif

#include "nba/config.hpp"
#include "nba/core.hpp"
#include "nba/device/video_device.hpp"
#include "nba/integer.hpp"
#include "nba/rom/rom.hpp"

namespace {

constexpr size_t kFramePixels = 240U * 160U;
constexpr size_t kMaxRomBytes = 32U * 1024U * 1024U;

class CopyVideoDevice final : public nba::VideoDevice {
public:
  void Draw(nba::u32* buffer) override {
    if (buffer == nullptr) {
      return;
    }
    std::copy_n(buffer, kFramePixels, frame_.begin());
  }

  const uint32_t* Data() const {
    return frame_.data();
  }

private:
  std::array<uint32_t, kFramePixels> frame_{};
};

struct GBACoreHandleImpl {
  std::shared_ptr<nba::Config> config;
  std::shared_ptr<CopyVideoDevice> video_device;
  std::unique_ptr<nba::CoreBase> core;
  std::string last_error;

  GBACoreHandleImpl() {
    video_device = std::make_shared<CopyVideoDevice>();

    config = std::make_shared<nba::Config>();
    config->skip_bios = true;
    config->video_dev = video_device;

    core = nba::CreateCore(config);
  }
};

bool ReadROMFile(const std::filesystem::path& path, std::vector<nba::u8>& out) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
    return false;
  }

  std::error_code ec;
  auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size < 192U || file_size > kMaxRomBytes) {
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

struct GBACoreHandle {
  GBACoreHandleImpl impl;
};

extern "C" {

GBACoreHandle* GBA_Create(void) {
  try {
    return new GBACoreHandle();
  } catch (...) {
    return nullptr;
  }
}

void GBA_Destroy(GBACoreHandle* handle) {
  delete handle;
}

bool GBA_LoadROMFromPath(GBACoreHandle* handle, const char* rom_path) {
  if (handle == nullptr || rom_path == nullptr || rom_path[0] == '\0') {
    return false;
  }

  auto& impl = handle->impl;
  impl.last_error.clear();

  std::vector<nba::u8> rom_data;
  if (!ReadROMFile(std::filesystem::path(rom_path), rom_data)) {
    impl.last_error = "failed to read ROM file";
    return false;
  }

  try {
    impl.core = nba::CreateCore(impl.config);
    impl.core->Attach(nba::ROM{std::move(rom_data), std::unique_ptr<nba::Backup>{}, std::unique_ptr<nba::GPIO>{}});
    impl.core->Reset();
    return true;
  } catch (const std::exception& ex) {
    impl.last_error = ex.what();
  } catch (...) {
    impl.last_error = "unknown exception while loading ROM";
  }

  return false;
}

void GBA_StepFrame(GBACoreHandle* handle) {
  if (handle == nullptr || !handle->impl.core) {
    return;
  }

  try {
    handle->impl.core->RunForOneFrame();
  } catch (const std::exception& ex) {
    handle->impl.last_error = ex.what();
  } catch (...) {
    handle->impl.last_error = "unknown exception while stepping frame";
  }
}

const uint32_t* GBA_GetFrameBufferRGBA(GBACoreHandle* handle, size_t* pixel_count) {
  if (pixel_count != nullptr) {
    *pixel_count = kFramePixels;
  }

  if (handle == nullptr || !handle->impl.video_device) {
    return nullptr;
  }

  return handle->impl.video_device->Data();
}

const char* GBA_GetLastError(GBACoreHandle* handle) {
  if (handle == nullptr || handle->impl.last_error.empty()) {
    return nullptr;
  }

  return handle->impl.last_error.c_str();
}

}  // extern "C"
