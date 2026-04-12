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
constexpr uint32_t kScreenWidth = 256U;
constexpr uint32_t kScreenHeight = 192U;
constexpr uint32_t kCombinedWidth = kScreenWidth * 2U;
std::mutex g_core_lock;
bool g_in_use = false;
bool g_core_initialized = false;

bool ReadBinaryFile(const char* path, std::vector<uint8_t>& out, std::string& last_error) {
  if (!path) return false;
  std::ifstream s(path, std::ios::binary | std::ios::ate);
  if (!s.is_open()) return false;
  std::streamsize sz = s.tellg();
  if (sz <= 0) return false;
  s.seekg(0, std::ios::beg);
  out.resize((size_t)sz);
  s.read((char*)out.data(), sz);
  return true;
}

void ClearFrame(Runtime& runtime) { std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0xFF000000U); }

bool CopyFramebuffer(Runtime& runtime, std::string&) {
  int front = GPU::FrontBuffer;
  if (front < 0 || front > 1) return false;
  const uint32_t* top = GPU::Framebuffer[front][0];
  const uint32_t* bottom = GPU::Framebuffer[front][1];
  if (!top && !bottom) return false;
  for (size_t y = 0; y < kScreenHeight; ++y) {
    uint32_t* row = runtime.frame_rgba.data() + (y * kCombinedWidth);
    if (top) std::memcpy(row, top + (y * kScreenWidth), 256 * 4); else std::fill_n(row, kScreenWidth, 0xFF000000U);
    if (bottom) std::memcpy(row + kScreenWidth, bottom + (y * kScreenWidth), 256 * 4); else std::fill_n(row + kScreenWidth, kScreenWidth, 0xFF000000U);
  }
  return true;
}
}

std::unique_ptr<Runtime> CreateRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_in_use) return nullptr;
  if (!g_core_initialized) {
    Platform::Init(0, nullptr);
    Platform::SetConfigBool(Platform::ExternalBIOSEnable, true);
    Platform::SetConfigInt(Platform::AudioBitrate, 2);
    NDS::SetConsoleType(0);
    if (!NDS::Init()) { Platform::DeInit(); return nullptr; }
    GPU::InitRenderer(0);
    GPU::RenderSettings render_settings{};
    render_settings.Soft_Threaded = false;
    render_settings.GL_ScaleFactor = 1;
    render_settings.GL_BetterPolygons = false;
    GPU::SetRenderSettings(0, render_settings);
    g_core_initialized = true;
  }
  auto r = std::make_unique<Runtime>();
  r->initialized = true; ClearFrame(*r); g_in_use = true; return r;
}

bool LoadBIOSFromPath(Runtime& r, const char* p, std::string& err) {
  if (!r.initialized) return false;
  std::vector<uint8_t> d; if (!ReadBinaryFile(p, d, err)) return false;
  std::string res = std::filesystem::absolute(p).string();
  if (d.size() == 0x1000) { r.bios9_data = std::move(d); r.bios9_path = res; return true; }
  if (d.size() == 0x4000) { r.bios7_data = std::move(d); r.bios7_path = res; return true; }
  if (d.size() == 0x20000 || d.size() == 0x40000 || d.size() == 0x80000) { r.firmware_data = std::move(d); r.firmware_path = res; return true; }
  return false;
}

bool LoadROMFromPath(Runtime& r, const char* p, std::string& err) {
  std::vector<uint8_t> d; if (!ReadBinaryFile(p, d, err)) return false;
  return LoadROMFromMemory(r, d.data(), d.size(), err);
}

bool LoadROMFromMemory(Runtime& r, const void* d, size_t sz, std::string& err) {
  if (!r.initialized) return false;
  if (d == nullptr || sz == 0 || sz > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    err = "invalid ROM image";
    return false;
  }
  r.rom_data.assign((const uint8_t*)d, (const uint8_t*)d + sz);
  bool has_ext = !r.bios9_data.empty() && !r.bios7_data.empty();
  Platform::SetConfigBool(Platform::ExternalBIOSEnable, has_ext);
  if (has_ext) {
    Platform::SetConfigString(Platform::BIOS9Path, r.bios9_path);
    Platform::SetConfigString(Platform::BIOS7Path, r.bios7_path);
    Platform::SetConfigString(Platform::FirmwarePath, r.firmware_path);
  }
  NDS::LoadBIOS();
  r.rom_loaded = NDS::LoadCart(r.rom_data.data(), (uint32_t)r.rom_data.size(), nullptr, 0);
  if (NDS::NeedsDirectBoot()) NDS::SetupDirectBoot("game.nds");
  NDS::Start();
  return r.rom_loaded;
}

void StepFrame(Runtime& r, std::string& err) { if (r.rom_loaded) { NDS::RunFrame(); CopyFramebuffer(r, err); r.frame_counter++; } }
void SetKeyStatus(Runtime& r, int k, bool p) {
  if (k < 0 || (size_t)k >= r.key_state.size()) return;
  r.key_state[(size_t)k] = p;
  uint32_t m = 0x000003FFU;
  static const uint32_t b[] = {1, 2, 8, 4, 64, 128, 32, 16, 512, 256};
  for (size_t i = 0; i < 10; ++i) if (r.key_state[i]) m &= ~b[i];
  NDS::SetKeyMask(m);
}
const uint32_t* GetFrameBufferRGBA(Runtime& r, size_t* pc) { if (pc) *pc = kFramePixels; return r.frame_rgba.data(); }
bool SaveStateToBuffer(Runtime&, void*, size_t, size_t*, std::string&) { return false; }
bool LoadStateFromBuffer(Runtime&, const void*, size_t, std::string&) { return false; }
bool ApplyCheatCode(Runtime&, const char*, std::string&) { return false; }

void ReleaseRuntime() {
  std::lock_guard<std::mutex> guard(g_core_lock);
  if (g_core_initialized) { NDS::Stop(); NDS::DeInit(); Platform::DeInit(); g_core_initialized = false; }
  g_in_use = false;
}
}
