#include "runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace core::sameboy {

namespace {

constexpr size_t kFramePixels = 160U * 144U;

uint32_t EncodePixel(GB_gameboy_t*, uint8_t r, uint8_t g, uint8_t b) {
  return 0xFF000000U | (static_cast<uint32_t>(r) << 16U) | (static_cast<uint32_t>(g) << 8U) |
         static_cast<uint32_t>(b);
}

GB_key_t ToSameBoyKey(int key) {
  switch (key) {
    case 0: return GB_KEY_A;
    case 1: return GB_KEY_B;
    case 2: return GB_KEY_SELECT;
    case 3: return GB_KEY_START;
    case 4: return GB_KEY_RIGHT;
    case 5: return GB_KEY_LEFT;
    case 6: return GB_KEY_UP;
    case 7: return GB_KEY_DOWN;
    default: return GB_KEY_MAX;
  }
}

void ResetCore(Runtime& runtime) {
  if (runtime.gb != nullptr) {
    GB_free(runtime.gb);
    GB_dealloc(runtime.gb);
  }
  runtime.gb = GB_init(GB_alloc(), GB_MODEL_CGB_E);
  runtime.key_state.fill(false);
  std::fill(runtime.frame_rgba.begin(), runtime.frame_rgba.end(), 0U);
  if (runtime.gb != nullptr) {
    GB_set_rgb_encode_callback(runtime.gb, EncodePixel);
    GB_set_pixels_output(runtime.gb, runtime.frame_rgba.data());
  }
}

}  // namespace

std::unique_ptr<Runtime> CreateRuntime() {
  auto runtime = std::make_unique<Runtime>();
  ResetCore(*runtime);
  return runtime;
}

bool LoadROMFromPath(Runtime& runtime, const char* rom_path, std::string& last_error) {
  try {
    if (rom_path == nullptr || rom_path[0] == '\0') {
      last_error = "ROM path is empty";
      return false;
    }

    ResetCore(runtime);
    if (runtime.gb == nullptr) {
      last_error = "failed to allocate SameBoy runtime";
      return false;
    }

    if (GB_load_rom(runtime.gb, rom_path) != 0) {
      last_error = "failed to load GB ROM from path";
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    last_error = std::string("SameBoy runtime exception: ") + e.what();
    return false;
  } catch (...) {
    last_error = "SameBoy runtime exception: unknown";
    return false;
  }
}

bool LoadROMFromMemory(Runtime& runtime, const void* rom_data, size_t rom_size, std::string& last_error) {
  try {
    if (rom_data == nullptr || rom_size == 0) {
      last_error = "ROM image is invalid";
      return false;
    }

    ResetCore(runtime);
    if (runtime.gb == nullptr) {
      last_error = "failed to allocate SameBoy runtime";
      return false;
    }

    runtime.rom_storage.resize(rom_size);
    std::memcpy(runtime.rom_storage.data(), rom_data, rom_size);
    GB_load_rom_from_buffer(runtime.gb, runtime.rom_storage.data(), runtime.rom_storage.size());
    return true;
  } catch (const std::exception& e) {
    last_error = std::string("SameBoy runtime exception: ") + e.what();
    return false;
  } catch (...) {
    last_error = "SameBoy runtime exception: unknown";
    return false;
  }
}

void StepFrame(Runtime& runtime, std::string&) {
  if (runtime.gb == nullptr) {
    return;
  }
  GB_run_frame(runtime.gb);
}

void SetKeyStatus(Runtime& runtime, int key, bool pressed) {
  if (runtime.gb == nullptr) {
    return;
  }
  if (key < 0 || static_cast<size_t>(key) >= runtime.key_state.size()) {
    return;
  }
  runtime.key_state[static_cast<size_t>(key)] = pressed;
  const GB_key_t gb_key = ToSameBoyKey(key);
  if (gb_key != GB_KEY_MAX) {
    GB_set_key_state(runtime.gb, gb_key, pressed);
  }
}

const uint32_t* GetFrameBufferRGBA(Runtime& runtime, size_t* pixel_count) {
  if (pixel_count != nullptr) {
    *pixel_count = kFramePixels;
  }
  return runtime.frame_rgba.data();
}

bool SaveStateToBuffer(Runtime& runtime, void* out_buffer, size_t buffer_size, size_t* out_size, std::string& last_error) {
  if (runtime.gb == nullptr) {
    last_error = "SameBoy core is not initialized";
    return false;
  }
  const size_t required_size = GB_get_save_state_size(runtime.gb);
  if (out_size != nullptr) {
    *out_size = required_size;
  }
  if (out_buffer == nullptr) {
    return true;
  }
  if (buffer_size < required_size) {
    last_error = "state buffer is too small";
    return false;
  }
  GB_save_state_to_buffer(runtime.gb, static_cast<uint8_t*>(out_buffer));
  return true;
}

bool LoadStateFromBuffer(Runtime& runtime, const void* state_buffer, size_t state_size, std::string& last_error) {
  if (runtime.gb == nullptr) {
    last_error = "SameBoy core is not initialized";
    return false;
  }
  if (state_buffer == nullptr || state_size == 0) {
    last_error = "invalid save-state buffer";
    return false;
  }
  if (GB_load_state_from_buffer(runtime.gb, static_cast<const uint8_t*>(state_buffer), state_size) != 0) {
    last_error = "failed to load save-state buffer";
    return false;
  }
  return true;
}

bool ApplyCheatCode(Runtime& runtime, const char* cheat_code, std::string& last_error) {
  if (runtime.gb == nullptr) {
    last_error = "SameBoy core is not initialized";
    return false;
  }
  if (cheat_code == nullptr || cheat_code[0] == '\0') {
    last_error = "cheat code is empty";
    return false;
  }
  if (GB_import_cheat(runtime.gb, cheat_code, "", true) == nullptr) {
    last_error = "failed to apply GB cheat code";
    return false;
  }
  return true;
}

}  // namespace core::sameboy
