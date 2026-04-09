#include "emulator_core_c_api.h"

#include <memory>
#include <string>

#include "cores/gba/runtime.hpp"
#include "cores/nes_nintendulator/runtime.hpp"

struct EmulatorCoreHandle {
  EmulatorCoreType core_type;
  std::unique_ptr<core::gba::Runtime> gba;
  std::unique_ptr<core::nes_nintendulator::Runtime> nes_nintendulator;
  std::string last_error;
};

extern "C" {

const char* EmulatorCoreTypeName(EmulatorCoreType core_type) {
  switch (core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      return "gba";
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      return "nes_nintendulator";
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
      case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
        handle->nes_nintendulator = core::nes_nintendulator::CreateRuntime();
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

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      if (!handle->gba) {
        handle->last_error = "core runtime is not initialized";
        return false;
      }
      return core::gba::LoadBIOSFromPath(*handle->gba, bios_path, handle->last_error);
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      handle->last_error = "NES core does not use external BIOS";
      return false;
    default:
      handle->last_error = "unknown core type";
      return false;
  }
}

bool EmulatorCore_LoadROMFromPath(EmulatorCoreHandle* handle, const char* rom_path) {
  if (handle == nullptr) {
    return false;
  }

  handle->last_error.clear();

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      if (!handle->gba) {
        handle->last_error = "core runtime is not initialized";
        return false;
      }
      return core::gba::LoadROMFromPath(*handle->gba, rom_path, handle->last_error);
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      if (!handle->nes_nintendulator) {
        handle->last_error = "core runtime is not initialized";
        return false;
      }
      return core::nes_nintendulator::LoadROMFromPath(*handle->nes_nintendulator, rom_path, handle->last_error);
    default:
      handle->last_error = "unknown core type";
      return false;
  }
}

void EmulatorCore_StepFrame(EmulatorCoreHandle* handle) {
  if (handle == nullptr) {
    return;
  }

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      if (handle->gba) {
        core::gba::StepFrame(*handle->gba, handle->last_error);
      }
      break;
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      if (handle->nes_nintendulator) {
        core::nes_nintendulator::StepFrame(*handle->nes_nintendulator, handle->last_error);
      }
      break;
    default:
      break;
  }
}

void EmulatorCore_SetKeyStatus(EmulatorCoreHandle* handle, EmulatorKey key, bool pressed) {
  if (handle == nullptr) {
    return;
  }

  if (key < EMULATOR_KEY_A || key > EMULATOR_KEY_L) {
    return;
  }

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      if (handle->gba) {
        core::gba::SetKeyStatus(*handle->gba, static_cast<int>(key), pressed);
      }
      break;
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      if (handle->nes_nintendulator) {
        core::nes_nintendulator::SetKeyStatus(*handle->nes_nintendulator, static_cast<int>(key), pressed);
      }
      break;
    default:
      break;
  }
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
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      out_spec->width = 256;
      out_spec->height = 240;
      return true;
    default:
      return false;
  }
}

const uint32_t* EmulatorCore_GetFrameBufferRGBA(EmulatorCoreHandle* handle, size_t* pixel_count) {
  if (handle == nullptr) {
    if (pixel_count != nullptr) {
      *pixel_count = 0;
    }
    return nullptr;
  }

  switch (handle->core_type) {
    case EMULATOR_CORE_TYPE_GBA:
      if (!handle->gba) {
        break;
      }
      return core::gba::GetFrameBufferRGBA(*handle->gba, pixel_count);
    case EMULATOR_CORE_TYPE_NES_NINTENDULATOR:
      if (!handle->nes_nintendulator) {
        break;
      }
      return core::nes_nintendulator::GetFrameBufferRGBA(*handle->nes_nintendulator, pixel_count);
    default:
      break;
  }

  if (pixel_count != nullptr) {
    *pixel_count = 0;
  }
  return nullptr;
}

const char* EmulatorCore_GetLastError(EmulatorCoreHandle* handle) {
  if (handle == nullptr || handle->last_error.empty()) {
    return nullptr;
  }

  return handle->last_error.c_str();
}

}  // extern "C"
