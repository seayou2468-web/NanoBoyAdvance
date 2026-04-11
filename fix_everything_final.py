import os
import re

TEAKRA_INCLUDE_DIR = os.path.abspath('src/core/melonds/teakra/include/teakra')

def get_rel_path(from_file, header_name):
    header_abs = os.path.join(TEAKRA_INCLUDE_DIR, header_name)
    file_abs_dir = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(header_abs, file_abs_dir)
    return rel

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    teakra_headers = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    for line in lines:
        new_line = line
        # Look for #include "..." or #include <...> containing teakra/
        m = re.search(rb'#include\s*[<"]([^>"]*teakra/([^>"]+))[>"]', line)
        if m:
            full_include_path = m.group(1).decode()
            header_name = m.group(2).decode()
            if header_name in teakra_headers:
                rel = get_rel_path(file_path, header_name).encode()
                # Use " for relative includes
                new_line = re.sub(rb'[<"]' + re.escape(m.group(1)) + rb'[>"]', b'"' + rel + b'"', line)
                if new_line != line:
                    changed = True

        # Special case for teakra_c.cpp: Remove Teakra_GetDspMemory
        if b'teakra_c.cpp' in file_path.encode():
            if b'uint8_t* Teakra_GetDspMemory' in line:
                # We'll handle this by skipping lines in a second pass or just not adding this line
                changed = True
                continue

        new_lines.append(new_line)

    # Second pass for teakra_c.cpp to remove the function body
    if b'teakra_c.cpp' in file_path.encode() and changed:
        final_lines = []
        skip = False
        for l in new_lines:
            if b'uint8_t* Teakra_GetDspMemory' in l: # Should already be gone from first pass
                skip = True
                continue
            # If we were skipping and find the end of function
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                final_lines.append(l)
        new_lines = final_lines

    # Special case for teakra_c.h: Remove Teakra_GetDspMemory declaration
    if b'teakra_c.h' in file_path.encode():
        final_lines = [l for l in new_lines if b'Teakra_GetDspMemory' not in l]
        if len(final_lines) != len(new_lines):
            new_lines = final_lines
            changed = True

    # Special case for NDS.cpp: MELONDS_VERSION
    if b'NDS.cpp' in file_path.encode():
        if b'static char const emuID[16] = "melonDS " MELONDS_VERSION;' in data and b'#ifndef MELONDS_VERSION' not in data:
            old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
            replacement = b'''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;'''
            # Need to be careful with indentation and multiple lines
            new_data = data.replace(old, replacement)
            if new_data != data:
                with open(file_path, 'wb') as f:
                    f.write(new_data)
                print(f"Fixed NDS.cpp MELONDS_VERSION")
                return # Skip standard processing as we just did it

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Processed {file_path}")

for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            process_file(os.path.join(root, file))
