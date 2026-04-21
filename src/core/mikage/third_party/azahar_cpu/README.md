# Azahar CPU snapshot (CPU-only)

This directory vendors the ARM CPU implementation subtree from Azahar:

- Upstream: https://github.com/azahar-emu/azahar
- Commit: `2fff086e816b2e46561ca8ab396f7d81ded75ef6`
- Imported scope: `src/core/arm` and `src/common` (dependencies required by ARM CPU code)

Notes for this repository:

- The Mikage runtime keeps its existing `CPU` frontend API and currently executes through `src/core/mikage/src/core/CPU/cpu_interpreter.cpp`.
- Mikage CPU execution is consolidated onto the Azahar bridge path (legacy selectable CPU backend route removed).
- This vendored tree is intended for iterative wiring/migration of Mikage CPU execution paths against Azahar's interpreter components.
