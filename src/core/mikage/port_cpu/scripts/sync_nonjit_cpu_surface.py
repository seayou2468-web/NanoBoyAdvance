#!/usr/bin/env python3
"""Sync non-JIT CPU surface files from the vendored Cytrus snapshot into Mikage.

This script intentionally only updates the narrow file set tracked by
validate_porting_surface.py, so porting can be done incrementally and audibly.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[5]

PAIRS = {
    "src/core/mikage/core/arm/interpreter/armsupp.cpp": "src/core/mikage/port_cpu/cytrus/skyeye_common/armsupp.cpp",
    "src/core/mikage/core/arm/interpreter/vfp/vfp.cpp": "src/core/mikage/port_cpu/cytrus/skyeye_common/vfp/vfp.cpp",
    "src/core/mikage/core/arm/interpreter/vfp/vfpdouble.cpp": "src/core/mikage/port_cpu/cytrus/skyeye_common/vfp/vfpdouble.cpp",
    "src/core/mikage/core/arm/interpreter/vfp/vfpsingle.cpp": "src/core/mikage/port_cpu/cytrus/skyeye_common/vfp/vfpsingle.cpp",
    "src/core/mikage/core/arm/interpreter/vfp/vfpinstr.cpp": "src/core/mikage/port_cpu/cytrus/skyeye_common/vfp/vfpinstr.cpp",
    "src/core/mikage/core/arm/interpreter/arm_regformat.h": "src/core/mikage/port_cpu/cytrus/skyeye_common/arm_regformat.h",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="Only report drift; do not copy files")
    args = parser.parse_args()

    changed = 0
    for dst_rel, src_rel in PAIRS.items():
        src = REPO_ROOT / src_rel
        dst = REPO_ROOT / dst_rel

        if not src.exists() or not dst.exists():
            print(f"[MISSING] {dst_rel} <= {src_rel}")
            changed += 1
            continue

        if sha256(src) == sha256(dst):
            print(f"[OK] {dst_rel}")
            continue

        if args.check:
            print(f"[DIFF] {dst_rel}")
            changed += 1
            continue

        shutil.copy2(src, dst)
        print(f"[SYNC] {dst_rel} <= {src_rel}")
        changed += 1

    if args.check and changed:
        print(f"\nDetected {changed} file(s) out of sync.")
        return 1

    if changed:
        print(f"\nUpdated {changed} file(s).")
    else:
        print("\nAlready synchronized.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
