#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool EmulatorFrontend_IsSupportedROMPath(const char* rom_path);

bool EmulatorFrontend_LoadROMImageFromPath(const char* rom_path,
                                           uint8_t** out_data,
                                           size_t* out_size,
                                           char* out_error,
                                           size_t out_error_size);
void EmulatorFrontend_FreeROMImage(uint8_t* image_data);

bool EmulatorFrontend_LoadBinaryFile(const char* path,
                                     uint8_t** out_data,
                                     size_t* out_size,
                                     char* out_error,
                                     size_t out_error_size);
bool EmulatorFrontend_SaveBinaryFile(const char* path,
                                     const void* data,
                                     size_t size,
                                     char* out_error,
                                     size_t out_error_size);

bool EmulatorFrontend_LoadCheatText(const char* path,
                                    char** out_text,
                                    size_t* out_size,
                                    char* out_error,
                                    size_t out_error_size);
bool EmulatorFrontend_SaveCheatText(const char* path,
                                    const char* text,
                                    char* out_error,
                                    size_t out_error_size);
void EmulatorFrontend_FreeText(char* text);

#ifdef __cplusplus
}
#endif
