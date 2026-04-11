import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include')

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'rb') as f:
        content = f.read()

    # Target header filenames
    targets = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # Match #include "..." or #include <...>
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            include_path = m.group(1).decode()
            filename = os.path.basename(include_path)
            if filename in targets:
                # This is a teakra header. Calculate relative path.
                hdr_abs = os.path.join(TEAKRA_HDR_BASE, 'teakra', filename)
                file_dir_abs = os.path.abspath(os.path.dirname(file_path))
                rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')

                # Check if we should update
                if include_path != rel:
                    new_line = line.replace(m.group(0), b'#include "' + rel.encode() + b'"')
                    changed = True

        # Remove non-existent GetDspMemory from teakra_c.cpp
        if file_path.endswith('teakra_c.cpp'):
             if b'uint8_t* Teakra_GetDspMemory' in line:
                 changed = True
                 continue

        # Remove GetDspMemory from teakra_c.h
        if file_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                continue

        new_lines.append(new_line)

    # Post-process teakra_c.cpp to remove GetDspMemory function body
    if file_path.endswith('teakra_c.cpp') and changed:
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

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

# Fix melonDS files
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
