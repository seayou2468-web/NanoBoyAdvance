#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "../src/core/gba_core_c_api.h"

namespace {
constexpr std::size_t kWidth = 240;
constexpr std::size_t kHeight = 160;
constexpr std::size_t kPixels = kWidth * kHeight;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: %s <rom.gba> [bios.bin] [frames]\n", argv[0]);
    return 1;
  }

  const char* rom_path = argv[1];
  const char* bios_path = (argc >= 3) ? argv[2] : nullptr;
  const int frames = (argc >= 4) ? std::atoi(argv[3]) : 300;

  GBACoreHandle* core = GBA_Create();
  if (!core) {
    std::fprintf(stderr, "GBA_Create failed\n");
    return 1;
  }

  if (bios_path && bios_path[0] != '\0') {
    if (!GBA_LoadBIOSFromPath(core, bios_path)) {
      std::fprintf(stderr, "GBA_LoadBIOSFromPath failed: %s\n", GBA_GetLastError(core));
      GBA_Destroy(core);
      return 1;
    }
  }

  if (!GBA_LoadROMFromPath(core, rom_path)) {
    std::fprintf(stderr, "GBA_LoadROMFromPath failed: %s\n", GBA_GetLastError(core));
    GBA_Destroy(core);
    return 1;
  }

  std::size_t white_heavy_frames = 0;
  std::size_t changing_frames = 0;
  std::uint32_t last_sample = 0;
  bool has_last_sample = false;

  for (int f = 1; f <= frames; ++f) {
    GBA_StepFrame(core);

    std::size_t pixel_count = 0;
    const std::uint32_t* fb = GBA_GetFrameBufferRGBA(core, &pixel_count);
    if (!fb || pixel_count < kPixels) {
      std::fprintf(stderr, "framebuffer fetch failed at frame %d: %s\n", f, GBA_GetLastError(core));
      GBA_Destroy(core);
      return 1;
    }

    // 低コストサンプリング
    std::size_t sample_count = 0;
    std::size_t white_samples = 0;
    std::uint32_t hash = 2166136261u;
    constexpr std::size_t kStep = 353;
    for (std::size_t i = 0; i < kPixels; i += kStep) {
      const std::uint32_t px = fb[i];
      white_samples += (px == 0xFFFFFFFFu) ? 1u : 0u;
      hash ^= px;
      hash *= 16777619u;
      sample_count++;
    }

    if (sample_count > 0 && white_samples * 100u >= sample_count * 95u) {
      white_heavy_frames++;
    }

    if (has_last_sample && hash != last_sample) {
      changing_frames++;
    }
    last_sample = hash;
    has_last_sample = true;

    if ((f % 60) == 0) {
      std::printf("frame=%d white-heavy=%zu/%d changing=%zu\n",
                  f, white_heavy_frames, f, changing_frames);
    }
  }

  std::printf("\n=== diagnosis ===\n");
  std::printf("white-heavy frames: %zu / %d\n", white_heavy_frames, frames);
  std::printf("changing frame hashes: %zu / %d\n", changing_frames, frames);

  if (white_heavy_frames * 100u >= static_cast<std::size_t>(frames) * 95u) {
    std::printf("Likely core-side forced blank / ROM-BIOS dependency rather than Metal upload path.\n");
  } else if (changing_frames == 0) {
    std::printf("Frame content is not changing. Investigate CPU/PPU progression first.\n");
  } else {
    std::printf("Core framebuffer changes are observed. If iOS is still white, suspect Metal handoff/pixel format.\n");
  }

  GBA_Destroy(core);
  return 0;
}
