import os
import re

# Base directory for teakra headers (relative to repo root)
TEAKRA_HDR_ROOT = 'src/core/melonds/teakra/include'

def get_rel_include(from_file, include_path):
    # include_path is like "teakra/teakra.h"
    hdr_abs = os.path.abspath(os.path.join(TEAKRA_HDR_ROOT, include_path))
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(hdr_abs, file_dir_abs)
    return rel.replace('\\', '/')

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    # We want to catch:
    # #include "teakra/..."
    # #include <teakra/...>
    # and variations that might have been partially fixed

    # Regex to find any include that looks like it's trying to get a teakra header
    # We'll look for strings containing 'teakra/' followed by 'teakra.h', 'teakra_c.h', 'disassembler.h', or 'disassembler_c.h'
    targets = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    changed = False
    lines = data.splitlines(keepends=True)
    new_lines = []

    for line in lines:
        new_line = line
        # Match #include followed by anything in "" or <> that ends with one of our targets and contains teakra/
        m = re.search(rb'#include\s*[<"]([^>"]*teakra/([^>"]+))[>"]', line)
        if m:
            full_path = m.group(1).decode()
            header_name = m.group(2).decode()
            if header_name in targets:
                rel = get_rel_include(file_path, 'teakra/' + header_name)
                # Replace the whole path inside "" or <>
                # We'll use " for all relative includes
                pattern = rb'([<"])' + re.escape(m.group(1).replace(b'.', rb'\.').replace(b'/', rb'\/')) + rb'([>"])'
                new_line = re.sub(rb'([<"])' + re.escape(m.group(1)) + rb'([>"])', b'"' + rel.encode() + b'"', line)
                if new_line != line:
                    changed = True

        # Specific fix for teakra_c.cpp (GetDspMemory)
        if b'teakra_c.cpp' in file_path.encode():
            if b'uint8_t* Teakra_GetDspMemory' in line:
                changed = True
                continue

        new_lines.append(new_line)

    if b'teakra_c.cpp' in file_path.encode() and changed:
        final_lines = []
        skip = False
        for l in new_lines:
            if b'uint8_t* Teakra_GetDspMemory' in l:
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                final_lines.append(l)
        new_lines = final_lines

    # Fix teakra_c.h
    if b'teakra_c.h' in file_path.encode():
        final_lines = [l for l in new_lines if b'Teakra_GetDspMemory' not in l]
        if len(final_lines) != len(new_lines):
            new_lines = final_lines
            changed = True

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

# Run across the whole core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            process_file(os.path.join(root, file))

# Fix NDS.cpp MELONDS_VERSION
nds_path = 'src/core/melonds/NDS.cpp'
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
    print("Fixed NDS.cpp MELONDS_VERSION")
