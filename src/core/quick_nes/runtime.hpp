#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "./nes_emu/Nes_Emu.h"
#include "./nes_emu/Nes_State.h"

namespace core::quick_nes {

struct Runtime {
  std::unique_ptr<Nes_Emu> emu;
  std::vector<uint8_t> indexed_frame;
  size_t indexed_frame_row_bytes = 0;
  std::array<uint32_t, 256U * 240U> frame_rgba{};
  std::array<uint32_t, 256U> palette_rgba_lut{};
  std::array<bool, 10> key_state{};
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

}  // namespace core::quick_nes
