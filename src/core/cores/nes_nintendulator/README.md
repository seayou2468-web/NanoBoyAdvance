# NES Nintendulator core (imported/trimmed)

This directory introduces a new emulator-core slot based on code/assets fetched from:

- https://github.com/quietust/nintendulator/tree/master/src

## What was imported

`NES/` contains selected upstream source files for reference during porting.
They are intentionally not built directly because the original code targets Win32 APIs and project-specific glue.

## What is built now

`runtime.cpp` provides a **standard C++ only** runtime with no SDL/OpenGL/Win32 dependency.
Current scope is intentionally minimal:

- load `.nes` files from disk
- parse iNES header and PRG/CHR payloads
- provide a framebuffer/output surface through the shared C API

## Removed/disabled features from upstream context

- GUI / Win32 windowing integration
- debugger UI
- AVI/movie capture
- platform audio/video backends tied to external frameworks
- mapper/device plug-in glue that depends on upstream app infrastructure

This staged layout allows gradual replacement of placeholder runtime logic with real CPU/PPU/APU execution while keeping dependency surface small.
