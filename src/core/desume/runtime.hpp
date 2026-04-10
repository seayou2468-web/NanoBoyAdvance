#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace core::desume {

struct Runtime {
  std::array<bool, 10> key_state{};
  std::array<uint32_t, 256U * 384U> frame_rgba{};
  bool initialized = false;
  bool rom_loaded = false;
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error);
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);

}  // namespace core::desume
