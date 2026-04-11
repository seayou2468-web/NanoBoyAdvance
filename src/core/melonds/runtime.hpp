#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core::melonds {

struct Runtime {
  bool initialized = false;
  bool rom_loaded = false;
  std::vector<uint8_t> rom_data;
  std::vector<uint8_t> bios9_data;
  std::vector<uint8_t> bios7_data;
  std::string bios9_path;
  std::string bios7_path;
  std::array<uint32_t, 256U * 384U> frame_rgba{};
  std::array<bool, 10> key_state{};
  uint32_t frame_counter = 0;
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error);
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);
bool SaveStateToBuffer(Runtime& runtime, void* out_buffer, size_t buffer_size, size_t* out_size, std::string& last_error);
bool LoadStateFromBuffer(Runtime& runtime, const void* state_buffer, size_t state_size, std::string& last_error);
bool ApplyCheatCode(Runtime& runtime, const char* cheat_code, std::string& last_error);
void ReleaseRuntime();

}  // namespace core::melonds
