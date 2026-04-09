#include "emulator_core_c_api.h"

#include <memory>
#include <string>

#include "cores/gba/runtime.hpp"

struct EmulatorCoreHandle {
  EmulatorCoreType core_type;
  std::unique_ptr<core::gba::Runtime> gba;
  std::string last_error;
};

extern "C" {

const char* EmulatorCoreTypeName(EmulatorCoreType core_type) {
  switch (core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      return "gba";
    default:
      return nullptr;
  }
}

EmulatorCoreHandle* EmulatorCore_Create(EmulatorCoreType core_type) {
  try {
    auto* handle = new EmulatorCoreHandle();
    handle->core_type = core_type;

    switch (core_type) {
      case EMULATOR_CORE_TYPE_GBA:
        handle->gba = core::gba::CreateRuntime();
        return handle;
      default:
        delete handle;
        return nullptr;
    }
  } catch (...) {
    return nullptr;
  }
}

void EmulatorCore_Destroy(EmulatorCoreHandle* handle) {
  delete handle;
}

bool EmulatorCore_LoadBIOSFromPath(EmulatorCoreHandle* handle, const char* bios_path) {
  if (handle == nullptr) {
    return false;
  }

  handle->last_error.clear();
  if (!handle->gba) {
    handle->last_error = "core runtime is not initialized";
    return false;
  }

  return core::gba::LoadBIOSFromPath(*handle->gba, bios_path, handle->last_error);
}

bool EmulatorCore_LoadROMFromPath(EmulatorCoreHandle* handle, const char* rom_path) {
  if (handle == nullptr) {
    return false;
  }

  handle->last_error.clear();
  if (!handle->gba) {
    handle->last_error = "core runtime is not initialized";
    return false;
  }

  return core::gba::LoadROMFromPath(*handle->gba, rom_path, handle->last_error);
}

void EmulatorCore_StepFrame(EmulatorCoreHandle* handle) {
  if (handle == nullptr || !handle->gba) {
    return;
  }

  core::gba::StepFrame(*handle->gba, handle->last_error);
}

void EmulatorCore_SetKeyStatus(EmulatorCoreHandle* handle, EmulatorKey key, bool pressed) {
  if (handle == nullptr || !handle->gba) {
    return;
  }

  if (key < EMULATOR_KEY_A || key > EMULATOR_KEY_L) {
    return;
  }

  core::gba::SetKeyStatus(*handle->gba, static_cast<int>(key), pressed);
}

bool EmulatorCore_GetVideoSpec(const EmulatorCoreHandle* handle, EmulatorVideoSpec* out_spec) {
  if (handle == nullptr || out_spec == nullptr) {
    return false;
  }

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      out_spec->width = 240;
      out_spec->height = 160;
      return true;
    default:
      return false;
  }
}

const uint32_t* EmulatorCore_GetFrameBufferRGBA(EmulatorCoreHandle* handle, size_t* pixel_count) {
  if (handle == nullptr || !handle->gba) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }

  return core::gba::GetFrameBufferRGBA(*handle->gba, pixel_count);
}

const char* EmulatorCore_GetLastError(EmulatorCoreHandle* handle) {
  if (handle == nullptr || handle->last_error.empty()) {
    return nullptr;
  }

  return handle->last_error.c_str();
}

}  // extern "C"
