import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

# Mapping of file path to headers it should include (basenames)
FILE_MAP = {
    'src/core/melonds/DSi_DSP.cpp': ['teakra.h'],
    'src/core/melonds/teakra/src/teakra.cpp': ['teakra.h'],
    'src/core/melonds/teakra/src/teakra_c.cpp': ['teakra.h', 'teakra_c.h'],
    'src/core/melonds/teakra/src/disassembler.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/disassembler_c.cpp': ['disassembler.h', 'disassembler_c.h'],
    'src/core/melonds/teakra/src/parser.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/test_verifier/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/coff_reader/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/mod_test_generator/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/test_generator/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/step2_test_generator/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/makedsp1/main.cpp': ['disassembler.h'],
}

def get_relative_path(from_file, header_name):
    header_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(header_abs, file_dir).replace('\\', '/')

for file_path, headers in FILE_MAP.items():
    if not os.path.exists(file_path):
        continue

    with open(file_path, 'rb') as f:
        data = f.read()

    changed = False
    for h in headers:
        # Match #include followed by anything in "" or <> that ends with the header name
        # We want to be very precise to avoid messing up other includes
        # We'll look for strings like 'teakra/teakra.h' or just 'teakra.h' or any existing relative path
        pattern = rb'#include\s*[<"]([^>"]*' + h.encode() + rb')[>"]'

        rel_path = get_relative_path(file_path, h).encode()

        def replacement(match):
            nonlocal changed
            old_path = match.group(1)
            if old_path != rel_path:
                changed = True
                return b'#include "' + rel_path + b'"'
            return match.group(0)

        data = re.sub(pattern, replacement, data)

    # Specific C API fix for teakra_c.cpp
    if file_path == 'src/core/melonds/teakra/src/teakra_c.cpp':
        if b'uint8_t* Teakra_GetDspMemory' in data:
            lines = data.splitlines(keepends=True)
            new_lines = []
            skip = False
            for l in lines:
                if b'uint8_t* Teakra_GetDspMemory' in l:
                    skip = True
                    changed = True
                    continue
                if skip and l.strip() == b'}':
                    skip = False
                    continue
                if not skip:
                    new_lines.append(l)
            data = b''.join(new_lines)

    if changed:
        with open(file_path, 'wb') as f:
            f.write(data)
        print(f"Fixed {file_path}")

# Fix teakra_c.h (Removing GetDspMemory)
h_path = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
if os.path.exists(h_path):
    with open(h_path, 'rb') as f:
        h_lines = f.readlines()
    new_h_lines = [l for l in h_lines if b'Teakra_GetDspMemory' not in l]
    if len(new_h_lines) != len(h_lines):
        with open(h_path, 'wb') as f:
            f.writelines(new_h_lines)
        print(f"Fixed {h_path}")

# Fix NDS.cpp Version
nds_path = 'src/core/melonds/NDS.cpp'
if os.path.exists(nds_path):
    with open(nds_path, 'rb') as f:
        nds_data = f.read()
    if b'static char const emuID[16] = "melonDS " MELONDS_VERSION;' in nds_data and b'#ifndef MELONDS_VERSION' not in nds_data:
        old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
        replacement = b'''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;'''
        nds_data = nds_data.replace(old, replacement)
        with open(nds_path, 'wb') as f:
            f.write(nds_data)
        print(f"Fixed {nds_path}")
