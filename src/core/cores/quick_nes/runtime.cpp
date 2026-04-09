#include "./runtime.hpp"
#include "./nes_emu/Std_File_Reader.h"

#include <algorithm>
#include <array>
#include <exception>

namespace core::quick_nes {

namespace {

constexpr size_t kFramePixels = 256U * 240U;

int BuildJoypadMask(const std::array<bool, 10>& key_state) {
  int mask = 0;
  if (key_state[0]) mask |= 0x01;  // A
  if (key_state[1]) mask |= 0x02;  // B
  if (key_state[2]) mask |= 0x04;  // Select
  if (key_state[3]) mask |= 0x08;  // Start
  if (key_state[6]) mask |= 0x10;  // Up
  if (key_state[7]) mask |= 0x20;  // Down
  if (key_state[5]) mask |= 0x40;  // Left
  if (key_state[4]) mask |= 0x80;  // Right
  return mask;
}

uint32_t ToRGBA32(Nes_Emu::rgb_t c) {
  return 0xFF000000U | (static_cast<uint32_t>(c.red) << 16U) | (static_cast<uint32_t>(c.green) << 8U) |
         static_cast<uint32_t>(c.blue);
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  auto runtime = std::make_unique<Runtime>();
  runtime->emu = std::make_unique<Nes_Emu>();
  const auto indexed_height = static_cast<size_t>(runtime->emu->buffer_height());
  runtime->indexed_frame_row_bytes = static_cast<size_t>(Nes_Emu::buffer_width);
  runtime->indexed_frame.resize(runtime->indexed_frame_row_bytes * indexed_height, 0U);
  runtime->emu->set_pixels(runtime->indexed_frame.data(), Nes_Emu::buffer_width);
  return runtime;
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  try {
    if (rom_path == nullptr || rom_path[0] == '\0') {
      last_error = "ROM path is empty";
      return false;
    }

    runtime.emu = std::make_unique<Nes_Emu>();
    const auto indexed_height = static_cast<size_t>(runtime.emu->buffer_height());
    runtime.indexed_frame_row_bytes = static_cast<size_t>(Nes_Emu::buffer_width);
    runtime.indexed_frame.assign(runtime.indexed_frame_row_bytes * indexed_height, 0U);
    runtime.emu->set_pixels(runtime.indexed_frame.data(), Nes_Emu::buffer_width);
    runtime.key_state.fill(false);
    std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0U);

    Std_File_Reader rom_reader;
    if (const blargg_err_t open_err = rom_reader.open(rom_path)) {
      last_error = open_err;
      return false;
    }

    if (const blargg_err_t err = runtime.emu->load_ines(Auto_File_Reader(rom_reader))) {
      last_error = err;
      return false;
    }

    runtime.emu->set_sprite_mode(Nes_Emu::sprites_visible);
    runtime.emu->reset(true);
    return true;
  } catch (const std::exception& e) {
    last_error = std::string("NES runtime exception: ") + e.what();
    return false;
  } catch (...) {
    last_error = "NES runtime exception: unknown";
    return false;
  }
}

void StepFrame(Runtime& runtime, std::string& last_error) {
  if (!runtime.emu) {
    return;
  }

  const int joypad = BuildJoypadMask(runtime.key_state);
  if (const blargg_err_t err = runtime.emu->emulate_frame(joypad, 0)) {
    last_error = err;
    return;
  }

  const auto& frame = runtime.emu->frame();
  for (size_t y = 0; y < 240U; ++y) {
    const auto* row = frame.pixels + static_cast<ptrdiff_t>(y) * frame.pitch;
    for (size_t x = 0; x < 256U; ++x) {
      const uint8_t palette_index = row[x];
      const unsigned short color_index = frame.palette[palette_index];
      const auto rgb = Nes_Emu::nes_colors[color_index % Nes_Emu::color_table_size];
      runtime.frame_rgba[y * 256U + x] = ToRGBA32(rgb);
    }
  }
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

}  // namespace core::quick_nes
