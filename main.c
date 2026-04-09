#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include "src/core/emulator_core_c_api.h"

enum {
  WINDOW_SCALE = 3
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom.gba>\n", argv[0]);
    return 1;
  }

  EmulatorCoreHandle* core = EmulatorCore_Create(EMULATOR_CORE_TYPE_GBA);
  if (!core) {
    fprintf(stderr, "EmulatorCore_Create failed\n");
    return 1;
  }

  EmulatorVideoSpec video_spec = {0};
  if (!EmulatorCore_GetVideoSpec(core, &video_spec)) {
    fprintf(stderr, "EmulatorCore_GetVideoSpec failed\n");
    EmulatorCore_Destroy(core);
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    EmulatorCore_Destroy(core);
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "NanoBoyAdvance Linux Frontend",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      (int)video_spec.width * WINDOW_SCALE,
      (int)video_spec.height * WINDOW_SCALE,
      SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    EmulatorCore_Destroy(core);
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    EmulatorCore_Destroy(core);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_Texture* texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STREAMING,
      (int)video_spec.width,
      (int)video_spec.height);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    EmulatorCore_Destroy(core);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (!EmulatorCore_LoadROMFromPath(core, argv[1])) {
    const char* err = EmulatorCore_GetLastError(core);
    fprintf(stderr, "EmulatorCore_LoadROMFromPath failed: %s\n", err ? err : "unknown");
    EmulatorCore_Destroy(core);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        running = false;
      }
    }

    EmulatorCore_StepFrame(core);

    size_t pixel_count = 0;
    const uint32_t* pixels = EmulatorCore_GetFrameBufferRGBA(core, &pixel_count);
    const size_t expected_pixels = (size_t)video_spec.width * (size_t)video_spec.height;
    if (!pixels || pixel_count < expected_pixels) {
      const char* err = EmulatorCore_GetLastError(core);
      fprintf(stderr, "EmulatorCore_GetFrameBufferRGBA failed: %s\n", err ? err : "unknown");
      break;
    }

    SDL_UpdateTexture(texture, NULL, pixels, (int)video_spec.width * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
  }

  EmulatorCore_Destroy(core);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
