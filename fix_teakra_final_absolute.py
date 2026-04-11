import os
import re

REPO_ROOT = os.path.abspath('.')
HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

def get_rel(file_path, header_name):
    hdr_abs = os.path.join(HDR_BASE, header_name)
    file_dir = os.path.abspath(os.path.dirname(file_path))
    return os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

TARGETS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

# Process files
def process_file(f_path):
    with open(f_path, 'rb') as f:
        content = f.read()

    changed = False
    lines = content.splitlines(keepends=True)
    new_lines = []

    for line in lines:
        new_line = line
        # Match #include <...> or #include "..."
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path)
            if basename in TARGETS:
                # Calculate correct relative path
                rel = get_rel(f_path, basename)
                if inc_path != rel:
                    # Replace the path part
                    new_line = line.replace(m.group(1), rel.encode())
                    # Ensure it uses ""
                    new_line = new_line.replace(b'<', b'"').replace(b'>', b'"')
                    changed = True

        # GetDspMemory Removal from teakra_c.cpp
        if f_path.endswith('teakra_c.cpp'):
            if b'uint8_t* Teakra_GetDspMemory' in line:
                changed = True
                continue # Skip start of function

        new_lines.append(new_line)

    if f_path.endswith('teakra_c.cpp') and b'uint8_t* Teakra_GetDspMemory' in content:
        final_lines = []
        skip = False
        for l in new_lines:
            if b'uint8_t* Teakra_GetDspMemory' in l: # Should not happen here but safety
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                final_lines.append(l)
        new_lines = final_lines

    if changed:
        with open(f_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {f_path}")

# Search for files
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            process_file(os.path.join(root, file))

# teakra_c.h GetDspMemory
h_path = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
if os.path.exists(h_path):
    with open(h_path, 'rb') as f:
        h_lines = f.readlines()
    new_h = [l for l in h_lines if b'Teakra_GetDspMemory' not in l]
    if len(new_h) != len(h_lines):
        with open(h_path, 'wb') as f:
            f.writelines(new_h)
        print(f"Fixed {h_path}")

# NDS.cpp MELONDS_VERSION
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
    print(f"Fixed {nds_path}")
