#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../../nba/include/nba/config.hpp"
#include "../../../nba/include/nba/core.hpp"
#include "../../../nba/include/nba/integer.hpp"

namespace core::gba {

class CopyVideoDevice;

struct Runtime {
  std::shared_ptr<nba::Config> config;
  std::shared_ptr<CopyVideoDevice> video_device;
  std::unique_ptr<nba::CoreBase> core;
  std::vector<u8> bios_data;
};

std::unique_ptr<Runtime> CreateRuntime();
bool LoadBIOSFromPath(Runtime& runtime, const char* bios_path, std::string& last_error);
bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error);
void StepFrame(Runtime& runtime, std::string& last_error);
void SetKeyStatus(Runtime& runtime, int key, bool pressed);
const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count);

}  // namespace core::gba
