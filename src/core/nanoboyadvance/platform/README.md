# GBA platform layer

This directory contains the platform-side support code for the GBA core.
It was moved from `src/platform/core` to keep GBA-specific code colocated
under `src/core/nanoboyadvance/`.

## Responsibility

- ROM/BIOS/save-state loading helpers
- Save-state writer helpers
- Emulator thread and frame limiter utilities
- Game DB and runtime configuration

## Architecture note

The frontend should connect through `src/core/emulator_core_c_api.*`.
This layer remains core-specific implementation detail for the GBA core.
