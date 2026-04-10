#include "./emulator_core_c_api.h"

#include <string>

#include "../core_adapter.hpp"

struct EmulatorCoreHandle {
  const core::CoreAdapter* adapter = nullptr;
  void* runtime = nullptr;
  std::string last_error;
};

extern "C" {

const char* EmulatorCoreTypeName(EmulatorCoreType core_type) {
  const core::CoreAdapter* adapter = core::FindCoreAdapter(core_type);
  return adapter ? adapter->name : nullptr;
}

EmulatorCoreHandle* EmulatorCore_Create(EmulatorCoreType core_type) {
  const core::CoreAdapter* adapter = core::FindCoreAdapter(core_type);
  if (adapter == nullptr) {
    return nullptr;
  }

  try {
    auto* handle = new EmulatorCoreHandle();
    handle->adapter = adapter;
    handle->runtime = adapter->create_runtime ? adapter->create_runtime() : nullptr;
    if (handle->runtime == nullptr) {
      delete handle;
      return nullptr;
    }
    return handle;
  } catch (...) {
    return nullptr;
  }
}

void EmulatorCore_Destroy(EmulatorCoreHandle* handle) {
  if (handle == nullptr) {
    return;
  }

  if (handle->adapter != nullptr && handle->adapter->destroy_runtime != nullptr) {
    handle->adapter->destroy_runtime(handle->runtime);
  }

  delete handle;
}

bool EmulatorCore_LoadBIOSFromPath(EmulatorCoreHandle* handle, const char* bios_path) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->load_bios_from_path == nullptr) {
    return false;
  }

  handle->last_error.clear();
  return handle->adapter->load_bios_from_path(handle->runtime, bios_path, handle->last_error);
}

bool EmulatorCore_LoadROMFromPath(EmulatorCoreHandle* handle, const char* rom_path) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->load_rom_from_path == nullptr) {
    return false;
  }

  handle->last_error.clear();
  return handle->adapter->load_rom_from_path(handle->runtime, rom_path, handle->last_error);
}

void EmulatorCore_StepFrame(EmulatorCoreHandle* handle) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->step_frame == nullptr) {
    return;
  }

  handle->adapter->step_frame(handle->runtime, handle->last_error);
}

void EmulatorCore_SetKeyStatus(EmulatorCoreHandle* handle, EmulatorKey key, bool pressed) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->set_key_status == nullptr) {
    return;
  }

  if (key < EMULATOR_KEY_A || key > EMULATOR_KEY_L) {
    return;
  }

  handle->adapter->set_key_status(handle->runtime, static_cast<int>(key), pressed);
}

bool EmulatorCore_GetVideoSpec(const EmulatorCoreHandle* handle, EmulatorVideoSpec* out_spec) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->get_video_spec == nullptr) {
    return false;
  }

  return handle->adapter->get_video_spec(out_spec);
}

const uint32_t* EmulatorCore_GetFrameBufferRGBA(EmulatorCoreHandle* handle, size_t* pixel_count) {
  if (handle == nullptr || handle->adapter == nullptr || handle->adapter->get_framebuffer_rgba == nullptr) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }

  return handle->adapter->get_framebuffer_rgba(handle->runtime, pixel_count);
}

const char* EmulatorCore_GetLastError(EmulatorCoreHandle* handle) {
  if (handle == nullptr || handle->last_error.empty()) {
    return nullptr;
  }

  return handle->last_error.c_str();
}

}  // extern "C"
