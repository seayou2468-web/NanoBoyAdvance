#include <SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "src/core/api/emulator_core_c_api.h"

namespace {
constexpr int kWindowScale = 2;

void SetKey(EmulatorCoreHandle* core, SDL_Scancode scancode, bool pressed) {
  switch (scancode) {
    case SDL_SCANCODE_X: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_A, pressed); break;
    case SDL_SCANCODE_Z: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_B, pressed); break;
    case SDL_SCANCODE_RSHIFT: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_SELECT, pressed); break;
    case SDL_SCANCODE_RETURN: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_START, pressed); break;
    case SDL_SCANCODE_RIGHT: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_RIGHT, pressed); break;
    case SDL_SCANCODE_LEFT: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_LEFT, pressed); break;
    case SDL_SCANCODE_UP: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_UP, pressed); break;
    case SDL_SCANCODE_DOWN: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_DOWN, pressed); break;
    case SDL_SCANCODE_A: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_L, pressed); break;
    case SDL_SCANCODE_S: EmulatorCore_SetKeyStatus(core, EMULATOR_KEY_R, pressed); break;
    default: break;
  }
}

bool IsFramebufferBlank(const uint32_t* pixels, size_t count) {
  if (!pixels || count == 0) {
    return true;
  }

  for (size_t i = 0; i < count; ++i) {
    if (pixels[i] != 0xFF000000u) {
      return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: %s <rom-path> [bios-or-sysdata-path ...]\n", argv[0]);
    return 1;
  }

  EmulatorCoreHandle* core = EmulatorCore_Create(EMULATOR_CORE_TYPE_3DS);
  if (!core) {
    std::fprintf(stderr, "Failed to create 3DS core\n");
    return 1;
  }

  for (int i = 2; i < argc; ++i) {
    EmulatorCore_LoadBIOSFromPath(core, argv[i]);
  }

  if (!EmulatorCore_LoadROMFromPath(core, argv[1])) {
    const char* err = EmulatorCore_GetLastError(core);
    std::fprintf(stderr, "Load ROM failed: %s\n", err ? err : "unknown error");
    EmulatorCore_Destroy(core);
    return 1;
  }

  EmulatorVideoSpec spec{};
  if (!EmulatorCore_GetVideoSpec(core, &spec) || spec.width == 0 || spec.height == 0) {
    std::fprintf(stderr, "Failed to query video spec\n");
    EmulatorCore_Destroy(core);
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    EmulatorCore_Destroy(core);
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("Aurora SDL Frontend", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        static_cast<int>(spec.width) * kWindowScale, static_cast<int>(spec.height) * kWindowScale,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    EmulatorCore_Destroy(core);
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    EmulatorCore_Destroy(core);
    return 1;
  }

  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                           static_cast<int>(spec.width), static_cast<int>(spec.height));
  if (!texture) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    EmulatorCore_Destroy(core);
    return 1;
  }

  bool running = true;
  uint64_t frame_counter = 0;
  bool warned_blank = false;

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running = false;
      } else if (ev.type == SDL_KEYDOWN) {
        if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          running = false;
        } else {
          SetKey(core, ev.key.keysym.scancode, true);
        }
      } else if (ev.type == SDL_KEYUP) {
        SetKey(core, ev.key.keysym.scancode, false);
      }
    }

    EmulatorCore_StepFrame(core);

    size_t pixel_count = 0;
    const uint32_t* pixels = EmulatorCore_GetFrameBufferRGBA(core, &pixel_count);
    if (!pixels || pixel_count != static_cast<size_t>(spec.width) * static_cast<size_t>(spec.height)) {
      const char* err = EmulatorCore_GetLastError(core);
      if (err) {
        std::fprintf(stderr, "Frame fetch failed: %s\n", err);
      }
      SDL_Delay(10);
      continue;
    }

    if (!warned_blank && frame_counter > 120 && IsFramebufferBlank(pixels, pixel_count)) {
      std::fprintf(stderr, "Warning: framebuffer remains fully black after %llu frames\n", static_cast<unsigned long long>(frame_counter));
      warned_blank = true;
    }

    SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(spec.width * sizeof(uint32_t)));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    ++frame_counter;
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  EmulatorCore_Destroy(core);
  return 0;
}
