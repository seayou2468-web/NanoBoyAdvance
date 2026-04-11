#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
import os
import pathlib
import re
import shlex
import subprocess
from concurrent.futures import ThreadPoolExecutor

roots = (
    pathlib.Path("src/core/desume"),
    pathlib.Path("src/core/nanoboyadvance"),
    pathlib.Path("src/core/quick_nes"),
    pathlib.Path("src/core/sameboy"),
)

exclude_patterns = (
    # Explicitly strip Lua / debug related units.
    "lua-engine.cpp",
    "debug.cpp",
    "gdbstub/",
    # Non-core frontends and platform-specific extras.
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

try:
    sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
except FileNotFoundError as exc:
    raise SystemExit("xcrun is required to use the iPhoneOS device SDK") from exc
if not sdk:
    raise SystemExit("Unable to resolve iPhoneOS SDK path from xcrun")

common_flags = [
    "-fsyntax-only",
    "-target", "arm64-apple-ios26.0",
    "-isysroot", sdk,
    "-miphoneos-version-min=26.0",
    # Disable JIT globally for every core compilation unit.
    "-UHAVE_JIT",
    "-DDISABLE_JIT=1",
]

include_flags = [
    "-Isrc/core/desume",
    "-Isrc/core",
    "-Isrc/core/nanoboyadvance/nba/include",
    "-Isrc/core/nanoboyadvance/platform/include",
]

cxx_base = ["clang++", "-std=c++20", *common_flags, *include_flags]
c_base = ["clang", "-std=gnu11", *common_flags, *include_flags]

files = []
for root in roots:
    for pattern in ("*.cpp", "*.c"):
        for path in root.rglob(pattern):
            normalized = str(path).replace("\\", "/")
            if any(pattern in normalized for pattern in exclude_patterns):
                continue
            files.append(normalized)
files.sort()

errors = []
warnings = 0
jobs = max(1, int(os.environ.get("DESUME_CHECK_JOBS", "1")))

def check_source(source: str):
    base = c_base if source.endswith(".c") else cxx_base
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
    fp.write(f"sdk=iphoneos\\n")
    fp.write(f"sdk_path={sdk}\\n")
    fp.write(f"compiled_units={len(files)}\\n")
    fp.write(f"failed_units={len(errors)}\\n")
    fp.write(f"error_count={error_count}\\n")
    fp.write(f"warning_count={warnings}\\n")
    fp.write("jit=disabled\\n")
    fp.write("excluded=lua-engine.cpp,debug.cpp\\n")
    fp.write(f"cxx_base={' '.join(shlex.quote(x) for x in cxx_base)}\\n")
    fp.write(f"c_base={' '.join(shlex.quote(x) for x in c_base)}\\n\\n")
    for source, text in errors:
        fp.write(f"## {source}\\n{text}\\n")

print(str(report))
print(f"compiled_units={len(files)} failed_units={len(errors)} error_count={error_count} warning_count={warnings}")
PY
