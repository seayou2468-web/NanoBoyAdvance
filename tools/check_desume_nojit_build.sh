#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
import pathlib
import re
import subprocess
import os
from concurrent.futures import ThreadPoolExecutor

root = pathlib.Path("src/core/desume")
exclude_patterns = (
    "utils/arm_jit/",
    "arm_jit.cpp",
    "lua-engine.cpp",
    "debug.cpp",
    "gdbstub/",
    "movie.cpp",
    "commandline.cpp",
    "frontend/",
    "metaspu/win32/",
    "3dnow_win.cpp",
    "cpu_detect_x86_win.cpp",
    "OGLRender",
    "GPU_Operations_AVX2.cpp",
    "GPU_Operations_SSE2.cpp",
    "GPU_Operations_NEON.cpp",
    "GPU_Operations_AltiVec.cpp",
    "utils/colorspacehandler/colorspacehandler_AVX2.cpp",
    "utils/colorspacehandler/colorspacehandler_AVX512.cpp",
    "utils/colorspacehandler/colorspacehandler_NEON.cpp",
    "utils/colorspacehandler/colorspacehandler_AltiVec.cpp",
)

files = []
for path in root.rglob("*.cpp"):
    normalized = str(path).replace("\\", "/")
    if any(pattern in normalized for pattern in exclude_patterns):
        continue
    files.append(normalized)
files.sort()

base = ["clang++", "-std=c++17", "-fsyntax-only", "-UHAVE_JIT", "-Isrc/core/desume"]
errors = []
warnings = 0
jobs = max(1, int(os.environ.get("DESUME_CHECK_JOBS", "1")))

def check_source(source: str):
    proc = subprocess.run(base + [source], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return source, proc.returncode, proc.stderr

with ThreadPoolExecutor(max_workers=jobs) as executor:
    for source, returncode, stderr in executor.map(check_source, files):
        warnings += len(re.findall(r": warning:", stderr))
        if returncode != 0:
            errors.append((source, stderr))

error_count = sum(len(re.findall(r": error:", text)) for _, text in errors)

report = pathlib.Path("build/desume_nojit_build_report.txt")
report.parent.mkdir(parents=True, exist_ok=True)
with report.open("w", encoding="utf-8") as fp:
    fp.write(f"compiled_units={len(files)}\n")
    fp.write(f"failed_units={len(errors)}\n")
    fp.write(f"error_count={error_count}\n")
    fp.write(f"warning_count={warnings}\n\n")
    for source, text in errors:
        fp.write(f"## {source}\n{text}\n")

print(str(report))
print(f"compiled_units={len(files)} failed_units={len(errors)} error_count={error_count} warning_count={warnings}")
PY
