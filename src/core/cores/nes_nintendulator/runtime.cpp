#include "runtime.hpp"

#include <filesystem>
#include <fstream>

namespace core::nes_nintendulator {

namespace {

constexpr size_t kFramePixels = 256U * 240U;

bool ReadWholeFile(const std::filesystem::path& path, std::vector<uint8_t>& out) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
    return false;
  }

  std::error_code ec;
  auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size < 16U) {
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



bool ParseINes(Runtime& runtime, std::string& last_error) {
  if (runtime.rom_image.size() < 16U) {
    last_error = "ROM is smaller than iNES header";
    return false;
  }

  const auto* h = runtime.rom_image.data();
  if (!(h[0] == 'N' && h[1] == 'E' && h[2] == 'S' && h[3] == 0x1A)) {
    last_error = "not an iNES ROM";
    return false;
  }

  const size_t prg_size = static_cast<size_t>(h[4]) * 16U * 1024U;
  const size_t chr_size = static_cast<size_t>(h[5]) * 8U * 1024U;
  const bool has_trainer = (h[6] & 0x04U) != 0;
  size_t cursor = 16U + (has_trainer ? 512U : 0U);

  const size_t required = cursor + prg_size + chr_size;
  if (required > runtime.rom_image.size()) {
    last_error = "truncated iNES ROM";
    return false;
  }

  runtime.prg_rom.assign(runtime.rom_image.begin() + static_cast<std::ptrdiff_t>(cursor),
                         runtime.rom_image.begin() + static_cast<std::ptrdiff_t>(cursor + prg_size));
  cursor += prg_size;
  runtime.chr_rom.assign(runtime.rom_image.begin() + static_cast<std::ptrdiff_t>(cursor),
                         runtime.rom_image.begin() + static_cast<std::ptrdiff_t>(cursor + chr_size));
  return true;
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  return std::make_unique<Runtime>();
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  if (rom_path == nullptr || rom_path[0] == '\0') {
    last_error = "ROM path is empty";
    return false;
  }

  runtime = Runtime{};
  if (!ReadWholeFile(std::filesystem::path(rom_path), runtime.rom_image)) {
    last_error = "failed to read ROM file";
    return false;
  }

  return ParseINes(runtime, last_error);
}

void StepFrame(Runtime& runtime, std::string&) {
  runtime.frame_counter++;

  // Minimal headless-safe renderer placeholder (no SDL/GL dependency).
  const uint8_t phase = static_cast<uint8_t>(runtime.frame_counter & 0xFFU);
  const uint8_t a = 0xFFU;
  const uint8_t r = phase;
  const uint8_t g = static_cast<uint8_t>(runtime.prg_rom.empty() ? 0x10U : 0x90U);
  const uint8_t b = static_cast<uint8_t>(runtime.chr_rom.empty() ? 0x10U : 0xE0U);
  const uint32_t pixel = (static_cast<uint32_t>(a) << 24U) |
                         (static_cast<uint32_t>(r) << 16U) |
                         (static_cast<uint32_t>(g) << 8U) |
                         static_cast<uint32_t>(b);

  runtime.frame_rgba.fill(pixel);
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

}  // namespace core::nes_nintendulator
