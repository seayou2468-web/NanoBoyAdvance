# NES core (dependency-trimmed runtime)

This core intentionally keeps only the runtime surface needed by the generic `EmulatorCore` API.

## Kept

- `.nes` ROM file load
- iNES header parse
- framebuffer output
- input state storage

## Removed

- upstream reference source dump files
- GUI/debugger/movie capture integration
- platform backend glue not required for core execution

The goal is to keep the repository small and focused while the NES runtime is iterated.
