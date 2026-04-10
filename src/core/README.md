# Core API layout

`src/core` now contains a generic API, adapter registry, and per-core implementations:

- `api/emulator_core_c_api.*`: generic frontend-facing API. Frontends should prefer this API.
- `core_adapter.*`: core adapter interface/registry used to connect each core runtime to the API.
- `cores/<core-name>/`: per-core implementation directory (runtime + optional compatibility wrapper).
- `emulator_core_c_api.h` / `gba_core_c_api.h`: backward-compatible include shim paths.

The generic C API (`api/emulator_core_c_api.*`) is the project-specific "core connection API".
Each emulator core plugs into this API (similar to a libretro-like frontend/core boundary),
so cores can be added/managed independently without changing frontend call sites.

## Current cores

- `cores/gba`: existing GBA runtime.
- `cores/gba/platform`: GBA-specific platform layer moved from `src/platform/core` so GBA-related
  loader/config/thread/save-state code is managed together with the GBA core.
- `cores/quick_nes`: NES core using the bundled QuickNES (`nes_emu`) implementation and runtime bridge.

## How to add a new core

1. Create `src/core/cores/<new-core>/` and add a runtime module similar to `cores/gba/runtime.*`.
2. Add a new value to `EmulatorCoreType` in `emulator_core_c_api.h`.
3. Add `<core>/core_adapter.cpp` and register it in `core_adapter_registry.cpp`.
4. Optional: add a compatibility wrapper API in the new core directory and a legacy include shim if old include paths must remain valid.

This keeps platform/frontend code stable while emulator cores evolve independently.
