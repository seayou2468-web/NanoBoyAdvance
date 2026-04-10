#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
import pathlib
import re
import subprocess

root = pathlib.Path("src/core/desume")
exclude_patterns = (
    "utils/arm_jit/",
    "arm_jit.cpp",
    "lua-engine.cpp",
    "movie.cpp",
    "commandline.cpp",
    "frontend/",
    "OGLRender",
    "GPU_Operations_AVX2.cpp",
    "GPU_Operations_SSE2.cpp",
    "GPU_Operations_NEON.cpp",
    "GPU_Operations_AltiVec.cpp",
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

for source in files:
    proc = subprocess.run(base + [source], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    warnings += len(re.findall(r": warning:", proc.stderr))
    if proc.returncode != 0:
        errors.append((source, proc.stderr))

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
