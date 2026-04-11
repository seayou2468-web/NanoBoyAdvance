import os
import re

# Repo root relative to this script's execution
REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    # Headers to redirect
    targets = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    for line in lines:
        new_line = line
        if b'#include' in line:
            # Match #include "..." or #include <...>
            m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
            if m:
                path_str = m.group(1).decode()
                filename = os.path.basename(path_str)
                if filename in targets:
                    # Found a target header
                    hdr_abs = os.path.join(TEAKRA_HDR_ROOT, filename)
                    file_dir_abs = os.path.abspath(os.path.dirname(file_path))

                    # Calculate relative path from file to header
                    rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')

                    if path_str != rel:
                        new_line = line.replace(m.group(1), rel.encode())
                        # Also force double quotes
                        new_line = new_line.replace(b'<', b'"').replace(b'>', b'"')
                        changed = True

        # C API Fixes
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

    if b'teakra_c.h' in file_path.encode():
        final_lines = [l for l in new_lines if b'Teakra_GetDspMemory' not in l]
        if len(final_lines) != len(new_lines):
            new_lines = final_lines
            changed = True

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            process_file(os.path.join(root, file))

# Fix NDS.cpp Version
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
