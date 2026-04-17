#!/usr/bin/env python3
"""Validate compatibility surface between Mikage legacy CPU and Cytrus CPU snapshot.

This focuses on paired files that should eventually become behaviorally equivalent.
Exit code:
  0 => all paired files are byte-identical
  1 => one or more paired files differ
"""

from __future__ import annotations

import hashlib
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


def line_count(path: Path) -> int:
    return path.read_text(encoding="utf-8", errors="ignore").count("\n") + 1


def main() -> int:
    print("=== Mikage/Cytrus paired-file drift report ===")
    mismatches = 0

    for legacy_rel, cytrus_rel in PAIRS.items():
        legacy = REPO_ROOT / legacy_rel
        cytrus = REPO_ROOT / cytrus_rel

        if not legacy.exists() or not cytrus.exists():
            print(f"[MISSING] {legacy_rel} <-> {cytrus_rel}")
            mismatches += 1
            continue

        same = sha256(legacy) == sha256(cytrus)
        legacy_lines = line_count(legacy)
        cytrus_lines = line_count(cytrus)

        status = "OK" if same else "DIFF"
        print(f"[{status}] {legacy_rel} ({legacy_lines} lines) <-> {cytrus_rel} ({cytrus_lines} lines)")

        if not same:
            mismatches += 1

    if mismatches:
        print(f"\nResult: {mismatches} paired files differ (porting work remains).")
        return 1

    print("\nResult: all paired files match.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
