#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include "src/core/gba_core_c_api.h"

enum {
  GBA_WIDTH = 240,
  GBA_HEIGHT = 160,
  WINDOW_SCALE = 3
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom.gba>\n", argv[0]);
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "NanoBoyAdvance Linux Frontend",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      GBA_WIDTH * WINDOW_SCALE,
      GBA_HEIGHT * WINDOW_SCALE,
      SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_Texture* texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STREAMING,
      GBA_WIDTH,
      GBA_HEIGHT);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  GBACoreHandle* core = GBA_Create();
  if (!core) {
    fprintf(stderr, "GBA_Create failed\n");
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (!GBA_LoadROMFromPath(core, argv[1])) {
    const char* err = GBA_GetLastError(core);
    fprintf(stderr, "GBA_LoadROMFromPath failed: %s\n", err ? err : "unknown");
    GBA_Destroy(core);
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

    GBA_StepFrame(core);

    size_t pixel_count = 0;
    const uint32_t* pixels = GBA_GetFrameBufferRGBA(core, &pixel_count);
    if (!pixels || pixel_count < (size_t)(GBA_WIDTH * GBA_HEIGHT)) {
      const char* err = GBA_GetLastError(core);
      fprintf(stderr, "GBA_GetFrameBufferRGBA failed: %s\n", err ? err : "unknown");
      break;
    }

    SDL_UpdateTexture(texture, NULL, pixels, GBA_WIDTH * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
  }

  GBA_Destroy(core);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
