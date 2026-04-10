#include "c_api.hpp"

#include <stdlib.h>

#include "gb.h"

struct SBA_Gameboy {
    GB_gameboy_t *gb;
};

static uint32_t EncodeARGB(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void) gb;
    return 0xFF000000U | ((uint32_t) r << 16U) | ((uint32_t) g << 8U) | (uint32_t) b;
}

SBA_Gameboy* SBA_create(SBA_Model model)
{
    SBA_Gameboy *wrapper = (SBA_Gameboy *) malloc(sizeof(*wrapper));
    if (wrapper == NULL) {
        return NULL;
    }
    wrapper->gb = GB_init(GB_alloc(), (GB_model_t) model);
    if (wrapper->gb == NULL) {
        free(wrapper);
        return NULL;
    }
    return wrapper;
}

void SBA_destroy(SBA_Gameboy* gb)
{
    if (gb == NULL) return;
    if (gb->gb != NULL) {
        GB_free(gb->gb);
        GB_dealloc(gb->gb);
        gb->gb = NULL;
    }
    free(gb);
}

int SBA_load_rom(SBA_Gameboy* gb, const char* path)
{
    if (gb == NULL || gb->gb == NULL) return -1;
    return GB_load_rom(gb->gb, path);
}

void SBA_load_rom_from_buffer(SBA_Gameboy* gb, const uint8_t* buffer, size_t size)
{
    if (gb == NULL || gb->gb == NULL) return;
    GB_load_rom_from_buffer(gb->gb, buffer, size);
}

uint64_t SBA_run_frame(SBA_Gameboy* gb)
{
    if (gb == NULL || gb->gb == NULL) return 0;
    return GB_run_frame(gb->gb);
}

void SBA_set_key_state(SBA_Gameboy* gb, SBA_Key index, bool pressed)
{
    if (gb == NULL || gb->gb == NULL) return;
    GB_set_key_state(gb->gb, (GB_key_t) index, pressed);
}

void SBA_use_default_argb_encoder(SBA_Gameboy* gb)
{
    if (gb == NULL || gb->gb == NULL) return;
    GB_set_rgb_encode_callback(gb->gb, EncodeARGB);
}

void SBA_set_pixels_output(SBA_Gameboy* gb, uint32_t* output)
{
    if (gb == NULL || gb->gb == NULL) return;
    GB_set_pixels_output(gb->gb, output);
}

size_t SBA_get_save_state_size(SBA_Gameboy* gb)
{
    if (gb == NULL || gb->gb == NULL) return 0;
    return GB_get_save_state_size(gb->gb);
}

void SBA_save_state_to_buffer(SBA_Gameboy* gb, uint8_t* buffer)
{
    if (gb == NULL || gb->gb == NULL || buffer == NULL) return;
    GB_save_state_to_buffer(gb->gb, buffer);
}

int SBA_load_state_from_buffer(SBA_Gameboy* gb, const uint8_t* buffer, size_t length)
{
    if (gb == NULL || gb->gb == NULL || buffer == NULL) return -1;
    return GB_load_state_from_buffer(gb->gb, buffer, length);
}

bool SBA_import_cheat(SBA_Gameboy* gb, const char* cheat_code)
{
    if (gb == NULL || gb->gb == NULL || cheat_code == NULL) return false;
    return GB_import_cheat(gb->gb, cheat_code, "", true) != NULL;
}
