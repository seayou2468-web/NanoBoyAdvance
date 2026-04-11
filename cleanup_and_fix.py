import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, h_name):
    h_abs = os.path.join(TEAKRA_HDR_BASE, h_name)
    f_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(h_abs, f_dir).replace('\\', '/')

def fix_teakra_includes_in_file(f_path):
    with open(f_path, 'rb') as f: data = f.read()
    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False
    for line in lines:
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            path_str = m.group(1).decode('utf-8', 'ignore')
            bname = os.path.basename(path_str)
            if bname in TARGET_HEADERS:
                correct = get_rel(f_path, bname)
                indent = line[:line.find(b'#include')]
                new_l = indent + b'#include "' + correct.encode() + b'"\n'
                if new_l.strip() != line.strip():
                    line = new_l
                    changed = True
        new_lines.append(line)
    if changed:
        with open(f_path, 'wb') as f: f.writelines(new_lines)
        return True
    return False

# 1. Fix all includes
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            fix_teakra_includes_in_file(os.path.join(root, file))

# 2. Fix NDS.cpp
nds_path = 'src/core/melonds/NDS.cpp'
with open(nds_path, 'rb') as f: lines = f.readlines()
new_lines = []
skip_next = 0
for i, line in enumerate(lines):
    if skip_next > 0:
        skip_next -= 1
        continue
    # Remove any previous RAM additions
    if b'MainRAM = new u8[' in line or b'SharedWRAM = new u8[' in line or b'ARM7WRAM = new u8[' in line:
        continue
    if b'delete[] MainRAM;' in line or b'delete[] SharedWRAM;' in line or b'delete[] ARM7WRAM;' in line:
        continue
    if b'#ifndef MELONDS_VERSION' in line:
        # Skip the next 3 lines too
        skip_next = 3
        continue

    # Re-apply correctly
    if b'ARM7 = new ARMv4();' in line:
        new_lines.append(line)
        new_lines.append(b'\n')
        new_lines.append(b'    MainRAM = new u8[MainRAMMaxSize];\n')
        new_lines.append(b'    SharedWRAM = new u8[SharedWRAMSize];\n')
        new_lines.append(b'    ARM7WRAM = new u8[ARM7WRAMSize];\n')
        continue
    if b'NDSCart::DeInit();' in line:
        new_lines.append(b'    delete[] MainRAM;\n')
        new_lines.append(b'    delete[] SharedWRAM;\n')
        new_lines.append(b'    delete[] ARM7WRAM;\n\n')
        new_lines.append(line)
        continue
    if b'static char const emuID[16] = "melonDS " MELONDS_VERSION;' in line:
        new_lines.append(b'#ifndef MELONDS_VERSION\n')
        new_lines.append(b'#define MELONDS_VERSION "unknown"\n')
        new_lines.append(b'#endif\n')
        new_lines.append(line)
        continue
    new_lines.append(line)
with open(nds_path, 'wb') as f: f.writelines(new_lines)

# 3. Fix runtime.cpp
rt_path = 'src/core/melonds/runtime.cpp'
with open(rt_path, 'rb') as f: lines = f.readlines()
new_lines = []
for line in lines:
    if b'GPU::InitRenderer(0);' in line: continue
    if b'NDS::LoadBIOS();' in line: continue

    if b'if (!NDS::Init()) {' in line:
        # We'll add GPU Init after the block
        new_lines.append(line)
        continue
    if b'Platform::DeInit();' in line:
        new_lines.append(line)
        continue
    if b'return nullptr;' in line and b'NDS::Init()' in lines[new_lines.index(new_lines[-1]) - 1].decode():
        new_lines.append(line)
        continue
    if b'}' in line and len(new_lines) > 2 and b'return nullptr;' in new_lines[-1] and b'NDS::Init()' in new_lines[-3]:
        new_lines.append(line)
        new_lines.append(b'    GPU::InitRenderer(0);\n')
        continue

    if b'runtime.rom_loaded = NDS::LoadCart' in line:
        new_lines.append(line)
        new_lines.append(b'  NDS::LoadBIOS();\n')
        continue

    new_lines.append(line)
with open(rt_path, 'wb') as f: f.writelines(new_lines)

# 4. Fix teakra_c.cpp/h (C API removal)
def fix_teakra_c_api():
    cpp = 'src/core/melonds/teakra/src/teakra_c.cpp'
    h = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
    if os.path.exists(cpp):
        with open(cpp, 'rb') as f: lines = f.readlines()
        new_l = []
        skip = False
        for line in lines:
            if b'Teakra_GetDspMemory' in line:
                skip = True
                continue
            if skip:
                if b'return' in line or b'}' in line:
                    if b'}' in line: skip = False
                    continue
            new_l.append(line)
        with open(cpp, 'wb') as f: f.writelines(new_l)
    if os.path.exists(h):
        with open(h, 'rb') as f: lines = f.readlines()
        new_l = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h, 'wb') as f: f.writelines(new_l)

fix_teakra_c_api()
