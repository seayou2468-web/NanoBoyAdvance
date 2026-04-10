# Core-centric repository structure

This repository is organized around a multi-emulator concept where each core is
independent and plugged into a shared API boundary.

## Layout

- `src/core/api/`
  - Public core API used by frontends (`emulator_core_c_api.*`, `gba_core_c_api.h` shim target).
- `src/core/core_adapter.*`
  - Adapter contract + registry that maps `EmulatorCoreType` to concrete core implementations.
- `src/core/gba/`
  - GBA runtime and GBA-specific compatibility API.
  - `platform/` keeps GBA-only platform helpers colocated with the core.
- `src/core/quick_nes/`
  - QuickNES runtime bridge and bundled emulator implementation.
- `src/core/emulator_core_c_api.h`, `src/core/gba_core_c_api.h`
  - Backward-compatible include shims.

## Why this structure

- Per-core changes stay local.
- Adding a new core is mostly:
  1. add runtime under `src/core/<new-core>/`
  2. implement a `<new-core>/core_adapter.cpp`
  3. register it in `core_adapter_registry.cpp`
- Frontends continue using one stable API surface.
