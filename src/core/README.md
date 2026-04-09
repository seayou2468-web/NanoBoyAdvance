# Core API layout

`src/core` now contains a generic API and per-core implementations:

- `emulator_core_c_api.*`: generic frontend-facing API. Frontends should prefer this API.
- `cores/<core-name>/`: per-core implementation directory (runtime + optional compatibility wrapper).
- `gba_core_c_api.h`: backward-compatible include path for existing GBA callers.

## Why this structure

The generic API decouples frontend code from one concrete emulator implementation. Each core can own its runtime details in `cores/<core-name>/`, so adding a new emulator does not bloat shared entrypoints.

## How to add a new core

1. Create `src/core/cores/<new-core>/` and add a runtime module similar to `cores/gba/runtime.*`.
2. Add a new value to `EmulatorCoreType` in `emulator_core_c_api.h`.
3. Extend `EmulatorCore_Create` and related dispatch in `emulator_core_c_api.cpp`.
4. Extend video spec and framebuffer dispatch for the new type.
5. Optional: add a compatibility wrapper API in the new core directory (like `cores/gba/gba_core_c_api.*`) and a legacy include shim if old include paths must remain valid.

This keeps platform/frontend code stable while emulator cores evolve independently.
