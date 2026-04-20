# host_memory (Panda3DS reference)

Source repository:
- https://github.com/wheremyfoodat/Panda3DS/tree/master/third_party/host_memory

This tree is intentionally iOS-focused for the Mikage core integration in this repository:
- Non-iOS platform backends from Panda3DS `host_memory.cpp` were not imported.
- The integrated implementation uses a safe fallback backing buffer and disables 4GiB fastmem virtual mirroring on iOS.

Imported/adapted files live in:
- `src/core/mikage/include/host_memory/`
- `src/core/mikage/src/core/host_memory/`
