#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CORE = ROOT / "src" / "core" / "cytrus" / "Core"

LOCAL_INCLUDE_ROOT = ROOT / "src" / "core" / "cytrus" / "Core" / "include"

ALLOWED_PREFIXES = (
    "<algorithm>", "<array>", "<atomic>", "<bit>", "<bitset>", "<cassert>", "<cctype>", "<cerrno>",
    "<cfenv>", "<cfloat>", "<charconv>", "<chrono>", "<climits>", "<cmath>", "<compare>",
    "<complex>", "<concepts>", "<condition_variable>", "<coroutine>", "<csetjmp>",
    "<csignal>", "<cstdarg>", "<cstddef>", "<cstdint>", "<cstdio>", "<cstdlib>",
    "<cstring>", "<ctime>", "<deque>", "<exception>", "<expected>", "<filesystem>",
    "<format>", "<forward_list>", "<fstream>", "<functional>", "<future>", "<initializer_list>",
    "<iomanip>", "<ios>", "<iosfwd>", "<iostream>", "<istream>", "<iterator>", "<latch>",
    "<limits>", "<list>", "<locale>", "<map>", "<memory>", "<memory_resource>", "<mutex>",
    "<new>", "<numbers>", "<numeric>", "<optional>", "<ostream>", "<queue>",
    "<random>", "<ranges>", "<ratio>", "<regex>", "<scoped_allocator>", "<semaphore>",
    "<set>", "<shared_mutex>", "<source_location>", "<span>", "<sstream>", "<stack>",
    "<stacktrace>", "<stdexcept>", "<stop_token>", "<streambuf>", "<string>",
    "<string_view>", "<syncstream>", "<system_error>", "<thread>", "<tuple>",
    "<type_traits>", "<typeindex>", "<typeinfo>", "<unordered_map>", "<unordered_set>",
    "<utility>", "<valarray>", "<variant>", "<vector>", "<version>", "<string.h>",
    "<Foundation/", "<CoreFoundation/", "<QuartzCore/", "<Metal/", "<UIKit/", "<AudioToolbox/",
    "<AudioUnit/", "<CommonCrypto/",
)

line_re = re.compile(r'^\s*#\s*include\s+([<\"].*[>\"])')
violations: list[str] = []

for path in sorted(CORE.rglob('*')):
    if path.suffix.lower() not in {'.h', '.hpp', '.hh', '.c', '.cc', '.cpp', '.mm'}:
        continue
    for idx, line in enumerate(path.read_text(encoding='utf-8', errors='ignore').splitlines(), start=1):
        m = line_re.match(line)
        if not m:
            continue
        header = m.group(1)
        if header.startswith('"'):
            continue
        if any(header.startswith(prefix) for prefix in ALLOWED_PREFIXES):
            continue
        if header.startswith("<boost/") or header.startswith("<fmt/"):
            local_rel = header[1:-1]
            if (LOCAL_INCLUDE_ROOT / local_rel).exists():
                continue
        violations.append(f"{path.relative_to(ROOT)}:{idx}: {header}")

if violations:
    print("Disallowed external includes found in cytrus Core:")
    for v in violations:
        print(v)
    sys.exit(1)

print("cytrus Core include scan passed (stdlib + iOS SDK only).")
