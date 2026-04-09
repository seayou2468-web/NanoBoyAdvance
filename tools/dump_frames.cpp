#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "src/core/emulator_core_c_api.h"

namespace {

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

uint32_t adler32(const uint8_t* data, size_t len) {
  constexpr uint32_t MOD_ADLER = 65521;
  uint32_t a = 1;
  uint32_t b = 0;
  for (size_t i = 0; i < len; ++i) {
    a = (a + data[i]) % MOD_ADLER;
    b = (b + a) % MOD_ADLER;
  }
  return (b << 16) | a;
}

void append_u32_be(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void write_chunk(std::vector<uint8_t>& png, const char type[4], const std::vector<uint8_t>& data) {
  append_u32_be(png, static_cast<uint32_t>(data.size()));
  const size_t type_offset = png.size();
  png.insert(png.end(), type, type + 4);
  png.insert(png.end(), data.begin(), data.end());
  const uint32_t crc = crc32(png.data() + type_offset, 4 + data.size());
  append_u32_be(png, crc);
}

bool write_rgba_png(const std::filesystem::path& path, uint32_t width, uint32_t height, const uint8_t* rgba) {
  std::vector<uint8_t> png;
  png.reserve(width * height * 5);

  static constexpr uint8_t kSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  png.insert(png.end(), std::begin(kSignature), std::end(kSignature));

  std::vector<uint8_t> ihdr;
  ihdr.reserve(13);
  append_u32_be(ihdr, width);
  append_u32_be(ihdr, height);
  ihdr.push_back(8);
  ihdr.push_back(6);
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  write_chunk(png, "IHDR", ihdr);

  const size_t row_bytes = static_cast<size_t>(width) * 4;
  const size_t raw_size = (row_bytes + 1) * height;
  std::vector<uint8_t> raw;
  raw.reserve(raw_size);
  for (uint32_t y = 0; y < height; ++y) {
    raw.push_back(0);
    const uint8_t* row = rgba + y * row_bytes;
    raw.insert(raw.end(), row, row + row_bytes);
  }

  std::vector<uint8_t> zdata;
  zdata.reserve(raw.size() + raw.size() / 65535 + 16);
  zdata.push_back(0x78);
  zdata.push_back(0x01);

  size_t offset = 0;
  while (offset < raw.size()) {
    const size_t block_len = std::min<size_t>(65535, raw.size() - offset);
    const bool is_final = (offset + block_len == raw.size());
    zdata.push_back(is_final ? 0x01 : 0x00);
    const uint16_t len = static_cast<uint16_t>(block_len);
    const uint16_t nlen = static_cast<uint16_t>(~len);
    zdata.push_back(static_cast<uint8_t>(len & 0xFF));
    zdata.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    zdata.push_back(static_cast<uint8_t>(nlen & 0xFF));
    zdata.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
    zdata.insert(zdata.end(), raw.begin() + offset, raw.begin() + offset + block_len);
    offset += block_len;
  }

  append_u32_be(zdata, adler32(raw.data(), raw.size()));
  write_chunk(png, "IDAT", zdata);
  write_chunk(png, "IEND", {});

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
  return static_cast<bool>(file);
}

EmulatorCoreType detect_core_type(const std::string& rom_path) {
  if (rom_path.size() >= 4 && rom_path.substr(rom_path.size() - 4) == ".nes") {
    return EMULATOR_CORE_TYPE_NES;
  }
  return EMULATOR_CORE_TYPE_GBA;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    std::cerr << "Usage: " << argv[0] << " <rom> <output_dir> <max_frames> <interval> <start_frame>\n";
    return 2;
  }

  const std::string rom_path = argv[1];
  const std::filesystem::path out_dir = argv[2];
  const int max_frames = std::atoi(argv[3]);
  const int interval = std::atoi(argv[4]);
  const int start_frame = std::atoi(argv[5]);
  if (max_frames <= 0 || interval <= 0 || start_frame < 0) {
    std::cerr << "Invalid numeric arguments\n";
    return 2;
  }

  std::error_code ec;
  std::filesystem::create_directories(out_dir, ec);
  if (ec) {
    std::cerr << "Failed to create output dir: " << ec.message() << "\n";
    return 1;
  }

  EmulatorCoreHandle* core = EmulatorCore_Create(detect_core_type(rom_path));
  if (!core) {
    std::cerr << "Failed to create core\n";
    return 1;
  }

  if (!EmulatorCore_LoadROMFromPath(core, rom_path.c_str())) {
    std::cerr << "Failed to load ROM: " << EmulatorCore_GetLastError(core) << "\n";
    EmulatorCore_Destroy(core);
    return 1;
  }

  EmulatorVideoSpec spec{};
  if (!EmulatorCore_GetVideoSpec(core, &spec)) {
    std::cerr << "Failed to get video spec\n";
    EmulatorCore_Destroy(core);
    return 1;
  }

  for (int frame = 1; frame <= max_frames; ++frame) {
    EmulatorCore_StepFrame(core);
    if (frame >= start_frame && frame % interval == 0) {
      size_t pixels = 0;
      const uint32_t* fb = EmulatorCore_GetFrameBufferRGBA(core, &pixels);
      if (!fb || pixels < static_cast<size_t>(spec.width) * spec.height) {
        std::cerr << "Framebuffer unavailable at frame " << frame << "\n";
        EmulatorCore_Destroy(core);
        return 1;
      }
      char filename[64];
      std::snprintf(filename, sizeof(filename), "frame_%04d.png", frame);
      const auto path = out_dir / filename;
      if (!write_rgba_png(path, spec.width, spec.height, reinterpret_cast<const uint8_t*>(fb))) {
        std::cerr << "Failed to write " << path << "\n";
        EmulatorCore_Destroy(core);
        return 1;
      }
      std::cout << "Wrote " << path << "\n";
    }
  }

  EmulatorCore_Destroy(core);
  return 0;
}
