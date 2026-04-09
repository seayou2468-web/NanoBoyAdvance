#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core::nes_nintendulator {

struct Runtime {
  std::vector<uint8_t> rom_image;
  std::vector<uint8_t> prg_rom;
  std::vector<uint8_t> chr_rom;
  std::array<uint32_t, 256U * 240U> frame_rgba{};
  std::array<bool, 10> key_state{};
  uint64_t frame_counter = 0;
  std::string upstream_origin;
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);

}  // namespace core::nes_nintendulator
