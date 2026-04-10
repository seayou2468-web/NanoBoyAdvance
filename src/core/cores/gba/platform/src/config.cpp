/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "../../../core/cores/gba/nba/include/nba/log.hpp"
#include <filesystem>
#include <fstream>
#include "../include/platform/config.hpp"

namespace nba {

void PlatformConfig::Load(std::string const& path) {
  if(!std::filesystem::exists(path)) {
    Save(path);
    return;
  }

  // Minimal std-only loader: preserve defaults unless file exists.
  // Legacy TOML parsing dependency has been removed.
  LoadCustomData();
}

void PlatformConfig::Save(std::string const& path) {
  std::ofstream file{path, std::ios::out | std::ios::trunc};
  if(!file.is_open()) {
    Log<Error>("Config: failed to open config path", path);
    return;
  }

  file << "# Standard-only configuration dump\n";
  file << "bios_path=" << bios_path << "\n";
  file << "save_folder=" << save_folder << "\n";
  file << "skip_bios=" << (skip_bios ? 1 : 0) << "\n";
  file << "volume=" << audio.volume << "\n";

  SaveCustomData(file);
}

} // namespace nba
