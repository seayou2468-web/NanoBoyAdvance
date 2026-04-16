# Cytrus CPU interpreter import (stage 1)

This directory vendors the ARM Skyeye CPU interpreter sources from:
- https://github.com/folium-app/Cytrus
- commit: `84f59c91a889fccd0c4a077f5c529c1836f40387`

Imported scope (CPU interpreter-related):
- `core/arm/skyeye_common/armsupp.cpp`
- `core/arm/skyeye_common/armstate.cpp`
- `core/arm/skyeye_common/vfp/*`
- `include/core/arm/skyeye_common/*`

Notes:
- This is an in-repo vendor snapshot to enable staged migration.
- Wiring these sources into the active Mikage runtime requires API/namespace/memory-system adaptation.
- Existing Mikage interpreter remains active until adapters are completed.
