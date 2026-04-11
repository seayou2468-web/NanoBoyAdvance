#include "frontend_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kZipLocalHeaderSignature = 0x04034b50u;

bool EndsWithIgnoreCase(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }

  const size_t offset = value.size() - suffix.size();
  for (size_t i = 0; i < suffix.size(); ++i) {
    const unsigned char a = static_cast<unsigned char>(value[offset + i]);
    const unsigned char b = static_cast<unsigned char>(suffix[i]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

void SetError(char* out_error, size_t out_error_size, const std::string& message) {
  if (out_error == nullptr || out_error_size == 0U) {
    return;
  }
  const size_t copy_len = std::min(out_error_size - 1U, message.size());
  std::memcpy(out_error, message.data(), copy_len);
  out_error[copy_len] = '\0';
}

bool ReadFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::string& error) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
    error = "file does not exist";
    return false;
  }

  std::error_code ec;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "failed to query file size";
    return false;
  }

  if (file_size > static_cast<uintmax_t>(std::numeric_limits<size_t>::max())) {
    error = "file is too large";
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    error = "failed to open file";
    return false;
  }

  out.resize(static_cast<size_t>(file_size));
  file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(file_size));
  if (!file.good() && !file.eof()) {
    error = "failed to read file";
    return false;
  }

  return true;
}

uint16_t ReadU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8U);
}

uint32_t ReadU32LE(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U) | (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}

bool ExtractStoredZipEntry(const std::vector<uint8_t>& zip,
                           std::vector<uint8_t>& out,
                           std::string& error,
                           std::string* out_name) {
  size_t offset = 0;
  while (offset + 30U <= zip.size()) {
    const uint8_t* hdr = zip.data() + offset;
    if (ReadU32LE(hdr) != kZipLocalHeaderSignature) {
      break;
    }

    const uint16_t flags = ReadU16LE(hdr + 6U);
    const uint16_t method = ReadU16LE(hdr + 8U);
    const uint32_t compressed_size = ReadU32LE(hdr + 18U);
    const uint32_t uncompressed_size = ReadU32LE(hdr + 22U);
    const uint16_t name_len = ReadU16LE(hdr + 26U);
    const uint16_t extra_len = ReadU16LE(hdr + 28U);

    const size_t header_and_name = 30U + static_cast<size_t>(name_len) + static_cast<size_t>(extra_len);
    if (offset + header_and_name > zip.size()) {
      error = "invalid zip header";
      return false;
    }

    const char* name_ptr = reinterpret_cast<const char*>(hdr + 30U);
    std::string name(name_ptr, name_ptr + name_len);
    const size_t data_offset = offset + header_and_name;

    if ((flags & 0x0008U) != 0U) {
      error = "zip data descriptor is not supported";
      return false;
    }

    if (data_offset + compressed_size > zip.size()) {
      error = "zip entry exceeds archive size";
      return false;
    }

    if (method == 0U && compressed_size == uncompressed_size && uncompressed_size > 0U) {
      out.assign(zip.begin() + static_cast<std::ptrdiff_t>(data_offset),
                 zip.begin() + static_cast<std::ptrdiff_t>(data_offset + compressed_size));
      if (out_name != nullptr) {
        *out_name = name;
      }
      return true;
    }

    offset = data_offset + compressed_size;
  }

  error = "zip archive has no supported (stored) entries";
  return false;
}

bool CopyVectorToMalloc(const std::vector<uint8_t>& src, uint8_t** out_data, size_t* out_size, std::string& error) {
  if (out_data == nullptr || out_size == nullptr) {
    error = "output pointers are null";
    return false;
  }

  *out_data = nullptr;
  *out_size = 0;

  if (src.empty()) {
    error = "image is empty";
    return false;
  }

  auto* raw = static_cast<uint8_t*>(std::malloc(src.size()));
  if (raw == nullptr) {
    error = "out of memory";
    return false;
  }

  std::memcpy(raw, src.data(), src.size());
  *out_data = raw;
  *out_size = src.size();
  return true;
}

}  // namespace

extern "C" {

bool EmulatorFrontend_IsSupportedROMPath(const char* rom_path) {
  if (rom_path == nullptr || rom_path[0] == '\0') {
    return false;
  }

  const std::string path(rom_path);
  return EndsWithIgnoreCase(path, ".gba") || EndsWithIgnoreCase(path, ".nes") ||
         EndsWithIgnoreCase(path, ".gb") || EndsWithIgnoreCase(path, ".gbc") ||
         EndsWithIgnoreCase(path, ".nds") || EndsWithIgnoreCase(path, ".zip");
}

bool EmulatorFrontend_LoadROMImageFromPath(const char* rom_path,
                                           uint8_t** out_data,
                                           size_t* out_size,
                                           char* out_error,
                                           size_t out_error_size) {
  if (out_data != nullptr) {
    *out_data = nullptr;
  }
  if (out_size != nullptr) {
    *out_size = 0;
  }

  if (rom_path == nullptr || rom_path[0] == '\0') {
    SetError(out_error, out_error_size, "ROM path is empty");
    return false;
  }

  std::vector<uint8_t> file_data;
  std::string error;
  if (!ReadFileBytes(std::filesystem::path(rom_path), file_data, error)) {
    SetError(out_error, out_error_size, error);
    return false;
  }

  std::vector<uint8_t> image;
  if (EndsWithIgnoreCase(rom_path, ".zip")) {
    std::string entry_name;
    if (!ExtractStoredZipEntry(file_data, image, error, &entry_name)) {
      SetError(out_error, out_error_size, error + " (std-only implementation supports zip method=store only)");
      return false;
    }
  } else {
    image = std::move(file_data);
  }

  if (!CopyVectorToMalloc(image, out_data, out_size, error)) {
    SetError(out_error, out_error_size, error);
    return false;
  }

  return true;
}

void EmulatorFrontend_FreeROMImage(uint8_t* image_data) {
  std::free(image_data);
}

bool EmulatorFrontend_LoadBinaryFile(const char* path,
                                     uint8_t** out_data,
                                     size_t* out_size,
                                     char* out_error,
                                     size_t out_error_size) {
  std::vector<uint8_t> file_data;
  std::string error;
  if (!ReadFileBytes(std::filesystem::path(path == nullptr ? "" : path), file_data, error)) {
    SetError(out_error, out_error_size, error);
    return false;
  }
  if (!CopyVectorToMalloc(file_data, out_data, out_size, error)) {
    SetError(out_error, out_error_size, error);
    return false;
  }
  return true;
}

bool EmulatorFrontend_SaveBinaryFile(const char* path,
                                     const void* data,
                                     size_t size,
                                     char* out_error,
                                     size_t out_error_size) {
  if (path == nullptr || path[0] == '\0') {
    SetError(out_error, out_error_size, "path is empty");
    return false;
  }
  if (data == nullptr && size != 0U) {
    SetError(out_error, out_error_size, "data is null");
    return false;
  }

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    SetError(out_error, out_error_size, "failed to open file for write");
    return false;
  }

  file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  if (!file.good()) {
    SetError(out_error, out_error_size, "failed to write file");
    return false;
  }

  return true;
}

bool EmulatorFrontend_LoadCheatText(const char* path,
                                    char** out_text,
                                    size_t* out_size,
                                    char* out_error,
                                    size_t out_error_size) {
  uint8_t* bytes = nullptr;
  size_t size = 0;
  if (!EmulatorFrontend_LoadBinaryFile(path, &bytes, &size, out_error, out_error_size)) {
    return false;
  }

  auto* text = static_cast<char*>(std::malloc(size + 1U));
  if (text == nullptr) {
    EmulatorFrontend_FreeROMImage(bytes);
    SetError(out_error, out_error_size, "out of memory");
    return false;
  }

  std::memcpy(text, bytes, size);
  text[size] = '\0';
  EmulatorFrontend_FreeROMImage(bytes);

  if (out_text != nullptr) {
    *out_text = text;
  }
  if (out_size != nullptr) {
    *out_size = size;
  }
  return true;
}

bool EmulatorFrontend_SaveCheatText(const char* path,
                                    const char* text,
                                    char* out_error,
                                    size_t out_error_size) {
  const size_t size = (text == nullptr) ? 0U : std::strlen(text);
  return EmulatorFrontend_SaveBinaryFile(path, text, size, out_error, out_error_size);
}

void EmulatorFrontend_FreeText(char* text) {
  std::free(text);
}

}  // extern "C"
