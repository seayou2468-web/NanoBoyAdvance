#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "src/core/emulator_core_c_api.h"

enum {
  WINDOW_SCALE = 3,
  NDS_SCREEN_HEIGHT = 192,
  NDS_SCREEN_GAP = 8
};

typedef struct {
  char bios9[1024];
  char bios7[1024];
  char firmware[1024];
} NDSPersistentConfig;

static void TrimTrailingWhitespace(char* text) {
  if (text == NULL) {
    return;
  }
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
    text[len - 1] = '\0';
    len--;
  }
}

static const char* NDSConfigPath(void) {
  static char path[1200];
  const char* home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return NULL;
  }
  (void)snprintf(path, sizeof(path), "%s/.config/nanoboyadvance/nds_paths.cfg", home);
  return path;
}

static void LoadNDSPersistentConfig(NDSPersistentConfig* config) {
  if (config == NULL) {
    return;
  }
  config->bios9[0] = '\0';
  config->bios7[0] = '\0';
  config->firmware[0] = '\0';

  const char* path = NDSConfigPath();
  if (path == NULL) {
    return;
  }
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return;
  }

  char line[2300];
  while (fgets(line, sizeof(line), file) != NULL) {
    TrimTrailingWhitespace(line);
    if (strncmp(line, "bios9=", 6) == 0) {
      (void)snprintf(config->bios9, sizeof(config->bios9), "%s", line + 6);
    } else if (strncmp(line, "bios7=", 6) == 0) {
      (void)snprintf(config->bios7, sizeof(config->bios7), "%s", line + 6);
    } else if (strncmp(line, "firmware=", 9) == 0) {
      (void)snprintf(config->firmware, sizeof(config->firmware), "%s", line + 9);
    }
  }

  fclose(file);
}

static void SaveNDSPersistentConfig(const NDSPersistentConfig* config) {
  if (config == NULL) {
    return;
  }
  const char* home = getenv("HOME");
  const char* path = NDSConfigPath();
  if (home == NULL || home[0] == '\0' || path == NULL) {
    return;
  }

  char config_dir[1100];
  (void)snprintf(config_dir, sizeof(config_dir), "%s/.config", home);
  (void)mkdir(config_dir, 0755);
  (void)snprintf(config_dir, sizeof(config_dir), "%s/.config/nanoboyadvance", home);
  (void)mkdir(config_dir, 0755);

  FILE* file = fopen(path, "w");
  if (file == NULL) {
    return;
  }
  fprintf(file, "bios9=%s\n", config->bios9);
  fprintf(file, "bios7=%s\n", config->bios7);
  fprintf(file, "firmware=%s\n", config->firmware);
  fclose(file);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom.(gba|nds|nes|gb|gbc)>\n", argv[0]);
    return 1;
  }

  EmulatorCoreType core_type = EMULATOR_CORE_TYPE_GBA;
  const char* ext = strrchr(argv[1], '.');
  if (ext != NULL && strcmp(ext, ".nes") == 0) {
    core_type = EMULATOR_CORE_TYPE_NES;
  }
  if (ext != NULL && strcmp(ext, ".nds") == 0) {
    core_type = EMULATOR_CORE_TYPE_NDS;
  }
  if (ext != NULL && (strcmp(ext, ".gb") == 0 || strcmp(ext, ".gbc") == 0)) {
    core_type = EMULATOR_CORE_TYPE_GB;
  }

  EmulatorCoreHandle* core = EmulatorCore_Create(core_type);
  if (!core) {
    fprintf(stderr, "EmulatorCore_Create failed\n");
    return 1;
  }

  if (core_type == EMULATOR_CORE_TYPE_NDS) {
    NDSPersistentConfig persisted = {0};
    LoadNDSPersistentConfig(&persisted);

    const char* bios9 = getenv("NDS_BIOS9_PATH");
    if ((bios9 == NULL || bios9[0] == '\0') && persisted.bios9[0] != '\0') {
      bios9 = persisted.bios9;
    }
    if (bios9 != NULL && bios9[0] != '\0') {
      if (!EmulatorCore_LoadBIOSFromPath(core, bios9)) {
        const char* err = EmulatorCore_GetLastError(core);
        fprintf(stderr, "EmulatorCore_LoadBIOSFromPath BIOS9 failed: %s\n", err ? err : "unknown");
        EmulatorCore_Destroy(core);
        return 1;
      }
      (void)snprintf(persisted.bios9, sizeof(persisted.bios9), "%s", bios9);
    }

    const char* bios7 = getenv("NDS_BIOS7_PATH");
    if ((bios7 == NULL || bios7[0] == '\0') && persisted.bios7[0] != '\0') {
      bios7 = persisted.bios7;
    }
    if (bios7 != NULL && bios7[0] != '\0') {
      if (!EmulatorCore_LoadBIOSFromPath(core, bios7)) {
        const char* err = EmulatorCore_GetLastError(core);
        fprintf(stderr, "EmulatorCore_LoadBIOSFromPath BIOS7 failed: %s\n", err ? err : "unknown");
        EmulatorCore_Destroy(core);
        return 1;
      }
      (void)snprintf(persisted.bios7, sizeof(persisted.bios7), "%s", bios7);
    }

    const char* firmware = getenv("NDS_FIRMWARE_PATH");
    if ((firmware == NULL || firmware[0] == '\0') && persisted.firmware[0] != '\0') {
      firmware = persisted.firmware;
    }
    if (firmware != NULL && firmware[0] != '\0') {
      if (!EmulatorCore_LoadBIOSFromPath(core, firmware)) {
        const char* err = EmulatorCore_GetLastError(core);
        fprintf(stderr, "EmulatorCore_LoadBIOSFromPath firmware failed: %s\n", err ? err : "unknown");
        EmulatorCore_Destroy(core);
        return 1;
      }
      (void)snprintf(persisted.firmware, sizeof(persisted.firmware), "%s", firmware);
    }

    SaveNDSPersistentConfig(&persisted);
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

  const int is_nds = (core_type == EMULATOR_CORE_TYPE_NDS);
  const int window_width = (int)video_spec.width * WINDOW_SCALE;
  const int window_height =
      is_nds ? ((NDS_SCREEN_HEIGHT * 2 + NDS_SCREEN_GAP) * WINDOW_SCALE) : ((int)video_spec.height * WINDOW_SCALE);

  SDL_Window* window = SDL_CreateWindow(
      "NanoBoyAdvance Linux Frontend",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      window_width,
      window_height,
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
      video_spec.pixel_format == EMULATOR_PIXEL_FORMAT_RGBA8888 ? SDL_PIXELFORMAT_RGBA8888 : SDL_PIXELFORMAT_ARGB8888,
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

    if (is_nds) {
      SDL_Rect src_top = {0, 0, (int)video_spec.width, NDS_SCREEN_HEIGHT};
      SDL_Rect src_bottom = {0, NDS_SCREEN_HEIGHT, (int)video_spec.width, NDS_SCREEN_HEIGHT};
      SDL_Rect dst_top = {0, 0, (int)video_spec.width * WINDOW_SCALE, NDS_SCREEN_HEIGHT * WINDOW_SCALE};
      SDL_Rect dst_bottom = {
          0,
          (NDS_SCREEN_HEIGHT + NDS_SCREEN_GAP) * WINDOW_SCALE,
          (int)video_spec.width * WINDOW_SCALE,
          NDS_SCREEN_HEIGHT * WINDOW_SCALE};

      SDL_RenderCopy(renderer, texture, &src_top, &dst_top);
      SDL_RenderCopy(renderer, texture, &src_bottom, &dst_bottom);
    } else {
      SDL_RenderCopy(renderer, texture, NULL, NULL);
    }

    SDL_RenderPresent(renderer);
  }

  EmulatorCore_Destroy(core);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
