#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "nes_emu/Nes_Emu.h"

namespace core::quick_nes {

struct Runtime {
  std::unique_ptr<Nes_Emu> emu;
  std::array<uint8_t, 256U * 240U> indexed_frame{};
  std::array<uint32_t, 256U * 240U> frame_rgba{};
  std::array<bool, 10> key_state{};
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);

}  // namespace core::quick_nes
