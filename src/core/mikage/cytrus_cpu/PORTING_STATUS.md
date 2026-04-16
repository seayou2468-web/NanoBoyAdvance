# Mikage ↔ Cytrus CPU Porting Status

## Goal
Replace Mikage's legacy Skyeye-based ARM11 interpreter (`core/arm/interpreter/*`) with the
vendored Cytrus CPU implementation (`cytrus_cpu/*`) while keeping current Mikage runtime behavior.

## Current status (this branch)
- Cytrus CPU sources are vendored in-tree and version-pinned (`84f59c91a889fccd0c4a077f5c529c1836f40387`).
- API/ABI parity validation tooling is now available via
  `scripts/validate_porting_surface.py`.
- A first-pass compatibility matrix has been produced to identify blockers before wiring runtime calls.

## Hard blockers identified
1. **`ARMul_State` model mismatch**
   - Mikage legacy CPU uses POD-like mutable global state assumptions.
   - Cytrus defines a constructor-backed `ARMul_State` that depends on `Core::System` and
     `Memory::MemorySystem` objects.
2. **Missing Mikage-side dependencies expected by Cytrus headers**
   - `core/gdbstub/gdbstub.h`
   - `Memory::MemorySystem` implementation/adapter layer
3. **Register enum divergence**
   - CP15 and VFP enum naming/order differ between legacy and Cytrus snapshots.
4. **MMU/coprocessor callback contracts differ**
   - Legacy interpreter uses function-style hooks.
   - Cytrus expects tighter integration with its memory-system abstractions.

## Porting phases

### Phase 1 — Vendor import ✅
Already complete.

### Phase 2 — Surface parity audit ✅
Use `scripts/validate_porting_surface.py` to diff public symbols and detect large API drifts.

### Phase 3 — Compatibility shim (next)
- Add a Mikage-local `core/gdbstub/gdbstub.h` shim with only the symbols required by Cytrus CPU.
- Add `Memory::MemorySystem` adapter facade backed by current Mikage `mem_map` read/write paths.
- Add CP15/VFP enum translation utilities.

### Phase 4 — Dual-path execution switch (next)
- Keep legacy interpreter as fallback.
- Add compile-time switch to run Cytrus CPU side-by-side in debug builds.
- Compare register/memory/PC traces for determinism checks.

### Phase 5 — Cutover
- Remove legacy interpreter only after parity test ROM corpus passes.

## Notes
A **full immediate drop-in replacement is not mechanically safe** without the shims above;
doing so would silently break memory and coprocessor behavior. This staged plan is designed to
reach a safe full migration.
