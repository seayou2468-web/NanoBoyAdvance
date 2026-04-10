#include "runtime.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <mutex>

#include "NDSSystem.h"
#include "GPU.h"
#include "utils/colorspacehandler/colorspacehandler.h"

namespace core::desume {

namespace {

constexpr size_t kScreenWidth = 256;
constexpr size_t kScreenHeight = 192;
constexpr size_t kCombinedHeight = kScreenHeight * 2;
constexpr size_t kFramePixels = kScreenWidth * kCombinedHeight;

std::mutex g_desmume_mutex;

bool EnsureInitialized(Runtime& runtime, std::string& last_error) {
  if (runtime.initialized) {
    return true;
  }

  Desmume_InitOnce();
  if (NDS_Init() != 0) {
    last_error = "failed to initialize DeSmuME core";
    return false;
  }

  if (GPU != nullptr) {
    GPU->SetColorFormat(NDSColorFormat_BGR555_Rev);
    GPU->SetCustomFramebufferSize(kScreenWidth, kScreenHeight);
  }
  CommonSettings.use_jit = false;

  runtime.initialized = true;
  return true;
}

void SyncPadState(const Runtime& runtime) {
  const bool right = runtime.key_state[4];
  const bool left = runtime.key_state[5];
  const bool down = runtime.key_state[7];
  const bool up = runtime.key_state[6];
  const bool select = runtime.key_state[2];
  const bool start = runtime.key_state[3];
  const bool b = runtime.key_state[1];
  const bool a = runtime.key_state[0];
  const bool l = runtime.key_state[9];
  const bool r = runtime.key_state[8];

  NDS_setPad(right, left, down, up, select, start, b, a, false, false, l, r, false, false);
}

void BlitDualScreen(Runtime& runtime) {
  if (GPU == nullptr) {
    std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0U);
    return;
  }

  const NDSDisplayInfo& info = GPU->GetDisplayInfo();
  const u16* main_buf = info.nativeBuffer16[NDSDisplayID_Main];
  const u16* touch_buf = info.nativeBuffer16[NDSDisplayID_Touch];
  if (main_buf == nullptr || touch_buf == nullptr) {
    std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0U);
    return;
  }

  for (size_t y = 0; y < kScreenHeight; ++y) {
    for (size_t x = 0; x < kScreenWidth; ++x) {
      const size_t src_idx = y * kScreenWidth + x;
      runtime.frame_rgba[y * kScreenWidth + x] = ColorspaceConvert555To8888Opaque<true>(main_buf[src_idx]);
      runtime.frame_rgba[(y + kScreenHeight) * kScreenWidth + x] = ColorspaceConvert555To8888Opaque<true>(touch_buf[src_idx]);
    }
  }
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  return std::make_unique<Runtime>();
}

bool LoadBIOSFromPath(Runtime&, const char*, std::string&) {
  return true;
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  std::lock_guard<std::mutex> lock(g_desmume_mutex);

  if (rom_path == nullptr || rom_path[0] == '\0') {
    last_error = "ROM path is empty";
    return false;
  }

  if (!std::filesystem::exists(std::filesystem::path(rom_path))) {
    last_error = "ROM path does not exist";
    return false;
  }

  if (!EnsureInitialized(runtime, last_error)) {
    return false;
  }

  if (NDS_LoadROM(rom_path, rom_path, rom_path) < 0) {
    last_error = "failed to load NDS ROM";
    return false;
  }

  NDS_Reset();
  runtime.rom_loaded = true;
  BlitDualScreen(runtime);
  return true;
}

bool LoadROMFromMemory(Runtime&, const void*, size_t, std::string& last_error) {
  last_error = "DeSmuME memory ROM loading is not implemented yet";
  return false;
}

void StepFrame(Runtime& runtime, std::string&) {
  std::lock_guard<std::mutex> lock(g_desmume_mutex);
  if (!runtime.initialized || !runtime.rom_loaded) {
    return;
  }

  SyncPadState(runtime);
  NDS_beginProcessingInput();
  NDS_exec<false>();
  NDS_endProcessingInput();
  BlitDualScreen(runtime);
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

}  // namespace core::desume
