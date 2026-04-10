#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C" {
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

struct SBA_Gameboy;
typedef struct SBA_Gameboy SBA_Gameboy;

typedef enum {
  SBA_MODEL_CGB_E = 0x205,
} SBA_Model;

typedef enum {
  SBA_KEY_RIGHT,
  SBA_KEY_LEFT,
  SBA_KEY_UP,
  SBA_KEY_DOWN,
  SBA_KEY_A,
  SBA_KEY_B,
  SBA_KEY_SELECT,
  SBA_KEY_START,
} SBA_Key;

SBA_Gameboy* SBA_create(SBA_Model model);
void SBA_destroy(SBA_Gameboy* gb);

int SBA_load_rom(SBA_Gameboy* gb, const char* path);
void SBA_load_rom_from_buffer(SBA_Gameboy* gb, const uint8_t* buffer, size_t size);
int SBA_load_boot_rom(SBA_Gameboy* gb, const char* path);
void SBA_load_boot_rom_from_buffer(SBA_Gameboy* gb, const uint8_t* buffer, size_t size);
uint64_t SBA_run_frame(SBA_Gameboy* gb);
void SBA_set_key_state(SBA_Gameboy* gb, SBA_Key index, bool pressed);

void SBA_use_default_argb_encoder(SBA_Gameboy* gb);
void SBA_set_pixels_output(SBA_Gameboy* gb, uint32_t* output);

size_t SBA_get_save_state_size(SBA_Gameboy* gb);
void SBA_save_state_to_buffer(SBA_Gameboy* gb, uint8_t* buffer);
int SBA_load_state_from_buffer(SBA_Gameboy* gb, const uint8_t* buffer, size_t length);

bool SBA_import_cheat(SBA_Gameboy* gb, const char* cheat_code);

#ifdef __cplusplus
}  // extern "C"
#endif
