import os
import re

REPO_ROOT = os.path.abspath('.')
HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

def process(f):
    with open(f, 'rb') as file:
        data = file.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    targets = {
        b'teakra.h': b'teakra.h',
        b'teakra_c.h': b'teakra_c.h',
        b'disassembler.h': b'disassembler.h',
        b'disassembler_c.h': b'disassembler_c.h'
    }

    for line in lines:
        new_line = line
        # Match #include "anything/teakra/header.h" or #include <anything/teakra/header.h>
        # or just #include "header.h" if it's one of ours
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            path = m.group(1)
            fname = os.path.basename(path.decode())
            if fname.encode() in targets:
                # Target found.
                hdr_abs = os.path.join(HDR_BASE, fname)
                file_dir = os.path.abspath(os.path.dirname(f))
                rel = os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

                if path != rel.encode():
                    new_line = line.replace(m.group(0), b'#include "' + rel.encode() + b'"')
                    changed = True

        # GetDspMemory Removal
        if f.endswith('teakra_c.cpp'):
            if b'uint8_t* Teakra_GetDspMemory' in line:
                changed = True
                continue

        new_lines.append(new_line)

    if f.endswith('teakra_c.cpp') and changed:
        final = []
        skip = False
        for l in new_lines:
            if b'uint8_t* Teakra_GetDspMemory' in l:
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                final.append(l)
        new_lines = final

    if changed:
        with open(f, 'wb') as file:
            file.writelines(new_lines)
        print(f"Fixed {f}")

files = [
    'src/core/melonds/teakra/src/disassembler_c.cpp',
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp',
    'src/core/melonds/teakra/src/teakra_c.cpp',
    'src/core/melonds/teakra/src/disassembler.cpp',
    'src/core/melonds/teakra/src/teakra.cpp',
    'src/core/melonds/teakra/src/test_verifier/main.cpp',
    'src/core/melonds/teakra/src/parser.cpp',
    'src/core/melonds/teakra/src/coff_reader/main.cpp',
    'src/core/melonds/DSi_DSP.cpp'
]

for f in files:
    if os.path.exists(f): process(f)

# teakra_c.h
h = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
with open(h, 'rb') as f:
    lines = f.readlines()
new_h = [l for l in lines if b'Teakra_GetDspMemory' not in l]
with open(h, 'wb') as f:
    f.writelines(new_h)

# NDS.cpp
nds = 'src/core/melonds/NDS.cpp'
with open(nds, 'rb') as f:
    data = f.read()
if b'static char const emuID[16] = "melonDS " MELONDS_VERSION;' in data and b'#ifndef MELONDS_VERSION' not in data:
    old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    rep = b'''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;'''
    data = data.replace(old, rep)
    with open(nds, 'wb') as f:
        f.write(data)
