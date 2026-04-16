#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/core/gba_core_c_api.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "Usage: %s <bios.bin> <rom.gba> <outdir>\n", argv[0]);
    return 1;
  }

  const char* bios = argv[1];
  const char* rom = argv[2];
  std::filesystem::path outdir = argv[3];
  std::filesystem::create_directories(outdir);

  GBACoreHandle* core = GBA_Create();
  if (!core) {
    std::fprintf(stderr, "GBA_Create failed\n");
    return 1;
  }

  if (!GBA_LoadBIOSFromPath(core, bios)) {
    std::fprintf(stderr, "GBA_LoadBIOSFromPath failed: %s\n", GBA_GetLastError(core));
    GBA_Destroy(core);
    return 1;
  }

  if (!GBA_LoadROMFromPath(core, rom)) {
    std::fprintf(stderr, "GBA_LoadROMFromPath failed: %s\n", GBA_GetLastError(core));
    GBA_Destroy(core);
    return 1;
  }

  for (int frame = 1; frame <= 500; ++frame) {
    GBA_StepFrame(core);

    if (frame % 50 == 0) {
      size_t count = 0;
      const uint32_t* fb = GBA_GetFrameBufferRGBA(core, &count);
      if (!fb || count < (240u * 160u)) {
        std::fprintf(stderr, "Framebuffer fetch failed at frame %d: %s\n", frame, GBA_GetLastError(core));
        GBA_Destroy(core);
        return 1;
      }

      std::filesystem::path raw = outdir / ("frame_" + std::to_string(frame) + ".raw");
      std::ofstream ofs(raw, std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(fb), static_cast<std::streamsize>(240 * 160 * sizeof(uint32_t)));
      std::printf("wrote %s\n", raw.string().c_str());
    }
  }

  GBA_Destroy(core);
  return 0;
}
