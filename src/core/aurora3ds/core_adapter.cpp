#if !defined(NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION)
#define NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION 0
#endif

#if NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION

#include "../core_adapter.hpp"

#include <array>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "./include/emulator.hpp"
#include "./include/memory.hpp"
#include "./include/services/hid.hpp"

namespace {

constexpr uint32_t kMikageFrameWidth = Emulator::width;
constexpr uint32_t kMikageFrameHeight = Emulator::height;

struct MikageRuntime {
  bool rom_loaded = false;
  std::vector<std::string> bios_paths;
  std::vector<std::string> pending_cheat_codes;
  std::vector<uint32_t> rgba_frame;
  std::filesystem::path temp_rom_path;
  std::unique_ptr<Emulator> emulator;
};

std::string Lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::optional<std::string> CanonicalSysdataNameForInputPath(const std::filesystem::path& path) {
  const std::string name = Lowercase(path.filename().string());
  if (name.find("aes_keys") != std::string::npos || name == "keys.txt") {
    return std::string("aes_keys.txt");
  }
  if (name.find("seeddb") != std::string::npos) {
    return std::string("seeddb.bin");
  }
  if (name.find("boot9") != std::string::npos) {
    return std::string("boot9.bin");
  }
  if (name.find("boot11") != std::string::npos) {
    return std::string("boot11.bin");
  }
  return std::nullopt;
}

bool StageSysdataFileIfRecognized(MikageRuntime* runtime, const std::filesystem::path& source_path,
                                  std::string& last_error) {
  if (!runtime || !runtime->emulator) {
    return false;
  }

  const auto canonical_name = CanonicalSysdataNameForInputPath(source_path);
  if (!canonical_name.has_value()) {
    return false;
  }

  std::error_code ec;
  const std::filesystem::path app_data_root = runtime->emulator->getAppDataRoot();
  const std::filesystem::path sysdata_dir = app_data_root / "sysdata";
  std::filesystem::create_directories(sysdata_dir, ec);
  if (ec) {
    last_error = "Failed to create sysdata directory for staged system files";
    return false;
  }

  const std::filesystem::path target = sysdata_dir / *canonical_name;
  std::filesystem::copy_file(source_path, target, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    last_error = "Failed to stage system file into app-data sysdata directory";
    return false;
  }

  return true;
}

void TryAutoStageSysdataFromCandidates(MikageRuntime* runtime,
                                       const std::vector<std::filesystem::path>& directories,
                                       std::string& last_error) {
  if (!runtime || !runtime->emulator) {
    return;
  }

  std::error_code ec;
  for (const auto& dir : directories) {
    if (dir.empty() || !std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
      continue;
    }

    // Scan only known filenames to keep this fast and deterministic.
    static constexpr const char* kCandidates[] = {
        "aes_keys.txt",
        "keys.txt",
        "seeddb.bin",
        "boot9.bin",
        "boot11.bin",
    };

    for (const char* name : kCandidates) {
      const std::filesystem::path candidate = dir / name;
      if (!std::filesystem::exists(candidate, ec) || !std::filesystem::is_regular_file(candidate, ec)) {
        continue;
      }

      std::string stage_error;
      if (!StageSysdataFileIfRecognized(runtime, candidate, stage_error)) {
        // Keep going for other candidates, but preserve the first non-empty error for diagnostics.
        if (!stage_error.empty() && last_error.empty()) {
          last_error = stage_error;
        }
      }
    }
  }
}

void* CreateRuntime() {
  auto* runtime = new MikageRuntime();
  runtime->rgba_frame.assign(static_cast<size_t>(kMikageFrameWidth) * static_cast<size_t>(kMikageFrameHeight),
                             0xFF000000u);

  try {
    runtime->emulator = std::make_unique<Emulator>();
  } catch (...) {
    delete runtime;
    return nullptr;
  }

  return runtime;
}

void RemoveTemporaryRomIfAny(MikageRuntime* runtime) {
  if (!runtime || runtime->temp_rom_path.empty()) {
    return;
  }

  std::error_code ec;
  std::filesystem::remove(runtime->temp_rom_path, ec);
  runtime->temp_rom_path.clear();
}

void DestroyRuntime(void* opaque_runtime) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    return;
  }

  RemoveTemporaryRomIfAny(runtime);
  runtime->emulator.reset();
  delete runtime;
}

bool LoadBiosFromPath(void* opaque_runtime, const char* bios_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !bios_path || bios_path[0] == '\0') {
    last_error = "Invalid 3DS BIOS path";
    return false;
  }

  std::filesystem::path path(bios_path);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    last_error = "3DS BIOS path does not exist";
    return false;
  }

  std::string file_name = Lowercase(path.filename().string());
  std::string extension = Lowercase(path.extension().string());
  const bool is_font_override = extension == ".bin" &&
                                (file_name.find("font") != std::string::npos ||
                                 file_name.find("shared_font") != std::string::npos);
  if (is_font_override) {
    SetSharedFontReplacementOverridePath(path);
    return true;
  }

  if (!StageSysdataFileIfRecognized(runtime, path, last_error) && !last_error.empty()) {
    return false;
  }

  runtime->bios_paths.emplace_back(bios_path);
  return true;
}

int DecodeHexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;
}

std::string DecodeFileUrlPath(std::string path) {
  std::string decoded;
  decoded.reserve(path.size());
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '%' && (i + 2) < path.size()) {
      const int hi = DecodeHexNibble(path[i + 1]);
      const int lo = DecodeHexNibble(path[i + 2]);
      if (hi >= 0 && lo >= 0) {
        decoded.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    decoded.push_back(path[i]);
  }
  return decoded;
}



static bool IsLikelySupportedRomPath(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext == ".3ds" || ext == ".cci" || ext == ".zcci" || ext == ".cxi" || ext == ".zcxi" || ext == ".app" || ext == ".ncch" ||
      ext == ".3dsx" || ext == ".z3dsx" || ext == ".elf" || ext == ".axf" || ext == ".cia" || ext == ".zcia") {
    return true;
  }

  if (ext == ".toml" || path.filename() == "config.toml") {
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  std::array<u8, 0x110> header{};
  file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  const std::streamsize read = file.gcount();
  if (read < 0x40) {
    return false;
  }

  const bool isElf = header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F';
  const bool isNcch = read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'C' && header[0x103] == 'H';
  const bool isNcsd = read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'S' && header[0x103] == 'D';
  const bool is3dsx = read >= 4 && header[0] == '3' && header[1] == 'D' && header[2] == 'S' && header[3] == 'X';
  const bool isCia = read >= 0x20 && header[0] == 0x20 && header[1] == 0x20 && header[2] == 0x00 && header[3] == 0x00;
  return isElf || isNcch || isNcsd || is3dsx || isCia;
}

enum class ContainerMagicType {
  Unknown = 0,
  CIA,
  NCSD,
  NCCH,
  ELF,
  _3DSX,
};

ContainerMagicType DetectContainerMagic(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return ContainerMagicType::Unknown;
  }
  std::array<u8, 0x220> header{};
  file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  const std::streamsize read = file.gcount();
  if (read < 4) {
    return ContainerMagicType::Unknown;
  }
  if (header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
    return ContainerMagicType::ELF;
  }
  if (read >= 0x20 && header[0] == 0x20 && header[1] == 0x20 && header[2] == 0x00 && header[3] == 0x00) {
    return ContainerMagicType::CIA;
  }
  if (header[0] == '3' && header[1] == 'D' && header[2] == 'S' && header[3] == 'X') {
    return ContainerMagicType::_3DSX;
  }
  if (read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'S' && header[0x103] == 'D') {
    return ContainerMagicType::NCSD;
  }
  if (read >= 0x104 && header[0x100] == 'N' && header[0x101] == 'C' && header[0x102] == 'C' && header[0x103] == 'H') {
    return ContainerMagicType::NCCH;
  }
  return ContainerMagicType::Unknown;
}

bool LoadRomFromPath(void* opaque_runtime, const char* rom_path, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator || !rom_path || rom_path[0] == '\0') {
    last_error = "Invalid 3DS ROM path";
    return false;
  }

  std::string path = rom_path;
  if (path.rfind("file://", 0) == 0) {
    path = path.substr(7);
    if (path.rfind("localhost/", 0) == 0) {
      path = path.substr(std::strlen("localhost"));
    }
    path = DecodeFileUrlPath(path);
  }

  std::filesystem::path fs_path(path);
  std::error_code ec;
  if (!std::filesystem::exists(fs_path, ec) || !std::filesystem::is_regular_file(fs_path, ec)) {
    last_error = "3DS ROM path does not exist or is not a file";
    return false;
  }

  if (!IsLikelySupportedRomPath(fs_path)) {
    last_error = "Selected file is not a supported ROM image (did you pass config.toml?)";
    return false;
  }

  const ContainerMagicType magic_type = DetectContainerMagic(fs_path);
  if (magic_type == ContainerMagicType::Unknown) {
    last_error = "ROM container parse precheck failed: CIA/NCSD/NCCH/ELF/3DSX magic not found. File may be corrupted or not fully extracted.";
    return false;
  }

  const std::filesystem::path app_data_root = runtime->emulator->getAppDataRoot();
  const std::filesystem::path bios_dir = app_data_root / "BIOS";

  // Best-effort auto-staging to reduce boot stalls caused by missing keys/seeddb.
  // We keep this non-fatal and still rely on explicit errors below if keys are absent.
  std::string autostage_error;
  std::vector<std::filesystem::path> auto_stage_dirs;
  auto_stage_dirs.emplace_back(fs_path.parent_path());
  if (!fs_path.parent_path().empty()) {
    auto_stage_dirs.emplace_back(fs_path.parent_path().parent_path());
  }
  auto_stage_dirs.emplace_back(bios_dir);
  auto_stage_dirs.emplace_back(app_data_root / "sysdata");
  TryAutoStageSysdataFromCandidates(runtime, auto_stage_dirs, autostage_error);

  const std::filesystem::path sysdata_dir = app_data_root / "sysdata";
  const bool has_aes_keys = std::filesystem::exists(sysdata_dir / "aes_keys.txt", ec);
  const bool has_seeddb = std::filesystem::exists(sysdata_dir / "seeddb.bin", ec);

  try {
    if (!runtime->emulator->loadROM(fs_path)) {
      if (!has_aes_keys) {
        if (!autostage_error.empty()) {
          last_error = std::string("ROM decryption failed: sysdata/aes_keys.txt is missing. Auto-stage attempt also failed: ") + autostage_error;
        } else {
          last_error =
              "ROM decryption failed: sysdata/aes_keys.txt is missing. Import AES keys and retry.";
        }
      } else {
        last_error = has_seeddb
                         ? "ROM load failed in NCCH/RomFS parse path after decryption. Verify ROM integrity/corruption and header validity."
                         : "ROM load failed in NCCH/RomFS parse path after decryption. Verify ROM extraction integrity and add sysdata/seeddb.bin if required.";
      }
      return false;
    }
  } catch (const std::exception& ex) {
    last_error = std::string("Mikage threw while loading 3DS ROM: ") + ex.what();
    return false;
  } catch (...) {
    last_error = "Mikage threw while loading 3DS ROM";
    return false;
  }

  runtime->rom_loaded = true;
  runtime->emulator->copyCompositeFrameRGBA(std::span<uint32_t>(runtime->rgba_frame));
  return true;
}

bool LoadRomFromMemory(void* opaque_runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    last_error = "Mikage runtime is not initialized";
    return false;
  }
  if (!rom_data || rom_size == 0) {
    last_error = "Invalid 3DS ROM memory buffer";
    return false;
  }

  RemoveTemporaryRomIfAny(runtime);

  std::error_code ec;
  const auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    last_error = "Failed to get temporary directory for 3DS ROM staging";
    return false;
  }

  const auto staging_file = temp_dir / "mikage_memrom.3ds";
  std::ofstream out(staging_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    last_error = "Failed to create temporary file for 3DS ROM staging";
    return false;
  }
  out.write(static_cast<const char*>(rom_data), static_cast<std::streamsize>(rom_size));
  if (!out.good()) {
    out.close();
    std::filesystem::remove(staging_file, ec);
    last_error = "Failed to write 3DS ROM memory buffer to temporary file";
    return false;
  }
  out.close();

  std::string path = staging_file.string();
  if (!LoadRomFromPath(runtime, path.c_str(), last_error)) {
    std::filesystem::remove(staging_file, ec);
    return false;
  }

  runtime->temp_rom_path = staging_file;
  return true;
}

uint32_t CoreKeyToHidMask(int key) {
  switch (key) {
    case 0: return HID::Keys::A;
    case 1: return HID::Keys::B;
    case 2: return HID::Keys::Select;
    case 3: return HID::Keys::Start;
    case 4: return HID::Keys::Right;
    case 5: return HID::Keys::Left;
    case 6: return HID::Keys::Up;
    case 7: return HID::Keys::Down;
    case 8: return HID::Keys::R;
    case 9: return HID::Keys::L;
    default: return HID::Keys::Null;
  }
}

void StepFrame(void* opaque_runtime, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator || !runtime->rom_loaded) {
    return;
  }

  try {
    runtime->emulator->runFrame();
  } catch (...) {
    last_error = "Mikage frame execution failed";
  }

  if (!runtime->emulator->copyCompositeFrameRGBA(std::span<uint32_t>(runtime->rgba_frame))) {
    if (last_error.empty()) {
      last_error = "Mikage failed to compose 3DS frame (both top/bottom framebuffers unavailable)";
    }
  }
}

void SetKeyStatus(void* opaque_runtime, int key, bool pressed) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime || !runtime->emulator) {
    return;
  }

  const uint32_t mask = CoreKeyToHidMask(key);
  if (mask == HID::Keys::Null) {
    return;
  }

  auto& hid = runtime->emulator->getServiceManager().getHID();
  hid.setKey(mask, pressed);
}

bool GetVideoSpec(EmulatorVideoSpec* out_spec) {
  if (!out_spec) {
    return false;
  }
  out_spec->width = kMikageFrameWidth;
  out_spec->height = kMikageFrameHeight;
  out_spec->pixel_format = EMULATOR_PIXEL_FORMAT_RGBA8888;
  return true;
}

const uint32_t* GetFrameBufferRGBA(void* opaque_runtime, size_t* pixel_count) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    if (pixel_count) *pixel_count = 0;
    return nullptr;
  }
  if (pixel_count) {
    *pixel_count = runtime->rgba_frame.size();
  }
  return runtime->rgba_frame.data();
}

bool SaveStateToBuffer(void*, void*, size_t, size_t*, std::string& last_error) {
  last_error = "Mikage savestates are not yet connected";
  return false;
}

bool LoadStateFromBuffer(void*, const void*, size_t, std::string& last_error) {
  last_error = "Mikage savestates are not yet connected";
  return false;
}

bool NormalizeCheatCode(const char* cheat_code, std::string& normalized, std::string& last_error) {
  if (!cheat_code || cheat_code[0] == '\0') {
    last_error = "Cheat code is empty";
    return false;
  }

  normalized = cheat_code;
  return true;
}

bool ApplyCheatCode(void* opaque_runtime, const char* cheat_code, std::string& last_error) {
  auto* runtime = static_cast<MikageRuntime*>(opaque_runtime);
  if (!runtime) {
    last_error = "Mikage runtime is not initialized";
    return false;
  }

  std::string normalized_cheat;
  if (!NormalizeCheatCode(cheat_code, normalized_cheat, last_error)) {
    return false;
  }

  runtime->pending_cheat_codes.push_back(std::move(normalized_cheat));
  return true;
}

}  // namespace

namespace core {

extern const CoreAdapter kMikageAdapter = {
    .name = "mikage",
    .type = EMULATOR_CORE_TYPE_3DS,
    .create_runtime = &CreateRuntime,
    .destroy_runtime = &DestroyRuntime,
    .load_bios_from_path = &LoadBiosFromPath,
    .load_rom_from_path = &LoadRomFromPath,
    .load_rom_from_memory = &LoadRomFromMemory,
    .step_frame = &StepFrame,
    .set_key_status = &SetKeyStatus,
    .get_video_spec = &GetVideoSpec,
    .get_framebuffer_rgba = &GetFrameBufferRGBA,
    .save_state_to_buffer = &SaveStateToBuffer,
    .load_state_from_buffer = &LoadStateFromBuffer,
    .apply_cheat_code = &ApplyCheatCode,
};

}  // namespace core

#endif  // NBA_ENABLE_MIKAGE_ADAPTER_IMPLEMENTATION
