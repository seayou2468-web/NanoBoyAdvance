# Cytrus CPU interpreter import

This directory vendors the ARM Skyeye CPU interpreter sources from:
- https://github.com/folium-app/Cytrus
- commit: `84f59c91a889fccd0c4a077f5c529c1836f40387`

Imported scope (CPU interpreter-related):
- `core/arm/skyeye_common/armsupp.cpp`
- `core/arm/skyeye_common/armstate.cpp`
- `core/arm/skyeye_common/vfp/*`
- `include/core/arm/skyeye_common/*`

## Migration status
The vendor import is complete, but active Mikage runtime wiring is still in progress.
See:
- `PORTING_STATUS.md` for phased migration plan and current blockers.
- `scripts/validate_porting_surface.py` for paired-file drift checks against
  Mikage's current legacy interpreter.

## Validation
Run:

```bash
python3 src/core/mikage/port_cpu/scripts/validate_porting_surface.py
```

The script returns a non-zero exit code while there is still CPU-surface drift,
which is expected until full cutover is complete.

## Surface sync helper
To copy the non-JIT paired files from the vendored Cytrus snapshot into
Mikage's active interpreter path, run:

```bash
python3 src/core/mikage/port_cpu/scripts/sync_nonjit_cpu_surface.py
```

For CI-only drift checks (without writing files), run:

```bash
python3 src/core/mikage/port_cpu/scripts/sync_nonjit_cpu_surface.py --check
```
