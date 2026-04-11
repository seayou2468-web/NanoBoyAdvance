import os
import re

TEAKRA_HDR_ROOT = os.path.abspath('src/core/melonds/teakra/include')

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    targets = [b'teakra.h', b'teakra_c.h', b'disassembler.h', b'disassembler_c.h']

    for line in lines:
        new_line = line
        if b'#include' in line and b'teakra' in line.lower():
            # Extract whatever is in "" or <>
            m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
            if m:
                path = m.group(1)
                filename = os.path.basename(path.decode())
                if filename.encode() in targets:
                    # Found a target header include
                    # Calculate correct relative path
                    hdr_abs = os.path.join(TEAKRA_HDR_ROOT, 'teakra', filename)
                    file_dir_abs = os.path.abspath(os.path.dirname(file_path))
                    rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')

                    if path != rel.encode():
                        new_line = line.replace(m.group(0), b'#include "' + rel.encode() + b'"')
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

# NDS.cpp Version
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
