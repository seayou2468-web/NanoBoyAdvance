# Core API layout

`src/core` now contains a generic API and per-core implementations:

- `emulator_core_c_api.*`: generic frontend-facing API. Frontends should prefer this API.
- `cores/<core-name>/`: per-core implementation directory (runtime + optional compatibility wrapper).
- `gba_core_c_api.h`: backward-compatible include path for existing GBA callers.

## Current cores

- `cores/gba`: existing GBA runtime.
- `cores/nes_nintendulator`: dependency-trimmed NES runtime scaffold with only core-facing functionality.

## How to add a new core

1. Create `src/core/cores/<new-core>/` and add a runtime module similar to `cores/gba/runtime.*`.
2. Add a new value to `EmulatorCoreType` in `emulator_core_c_api.h`.
3. Extend `EmulatorCore_Create` and related dispatch in `emulator_core_c_api.cpp`.
4. Extend video spec and framebuffer dispatch for the new type.
5. Optional: add a compatibility wrapper API in the new core directory and a legacy include shim if old include paths must remain valid.

This keeps platform/frontend code stable while emulator cores evolve independently.
