#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "c_api.hpp"

namespace core::sameboy {

struct Runtime {
  SBA_Gameboy* gb = nullptr;
  std::array<uint32_t, 160U * 144U> frame_rgba{};
  std::array<bool, 10> key_state{};
  std::vector<uint8_t> rom_storage;
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);
bool SaveStateToBuffer(Runtime& runtime, void* out_buffer, size_t buffer_size, size_t* out_size, std::string& last_error);
bool LoadStateFromBuffer(Runtime& runtime, const void* state_buffer, size_t state_size, std::string& last_error);
bool ApplyCheatCode(Runtime& runtime, const char* cheat_code, std::string& last_error);

}  // namespace core::sameboy
