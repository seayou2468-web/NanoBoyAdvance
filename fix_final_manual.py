import os
import re

# We will manually map the files and their needed relative paths to the headers
REPO_ROOT = os.path.abspath('.')
HDR_DIR = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

def get_rel(file_path, header_name):
    hdr_abs = os.path.join(HDR_DIR, header_name)
    file_dir = os.path.abspath(os.path.dirname(file_path))
    return os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

# Files and the teakra headers they include
files_to_fix = {
    'src/core/melonds/DSi_DSP.cpp': ['teakra.h'],
    'src/core/melonds/teakra/src/teakra.cpp': ['teakra.h'],
    'src/core/melonds/teakra/src/teakra_c.cpp': ['teakra.h', 'teakra_c.h'],
    'src/core/melonds/teakra/src/disassembler.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/disassembler_c.cpp': ['disassembler.h', 'disassembler_c.h'],
    'src/core/melonds/teakra/src/parser.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/test_verifier/main.cpp': ['disassembler.h'],
    'src/core/melonds/teakra/src/coff_reader/main.cpp': ['disassembler.h'],
}

for f, headers in files_to_fix.items():
    if not os.path.exists(f): continue
    with open(f, 'rb') as file:
        data = file.read()

    changed = False
    for h in headers:
        # Match any #include that ends with teakra/header.h or just header.h
        # e.g. "teakra/teakra.h", "../include/teakra/teakra.h", etc.
        pattern = rb'#include\s*[<"]([^>"]*' + h.encode() + rb')[>"]'
        rel = get_rel(f, h).encode()

        def rep(m):
            return b'#include "' + rel + b'"'

        new_data = re.sub(pattern, rep, data)
        if new_data != data:
            data = new_data
            changed = True

    if changed:
        with open(f, 'wb') as file:
            file.write(data)
        print(f"Fixed {f}")

# C API fixes
def fix_c_api():
    cpp = 'src/core/melonds/teakra/src/teakra_c.cpp'
    h = 'src/core/melonds/teakra/include/teakra/teakra_c.h'

    if os.path.exists(cpp):
        with open(cpp, 'rb') as f:
            lines = f.readlines()
        new_lines = []
        skip = False
        for l in lines:
            if b'uint8_t* Teakra_GetDspMemory' in l:
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                new_lines.append(l)
        with open(cpp, 'wb') as f:
            f.writelines(new_lines)

    if os.path.exists(h):
        with open(h, 'rb') as f:
            lines = f.readlines()
        new_lines = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h, 'wb') as f:
            f.writelines(new_lines)

fix_c_api()
