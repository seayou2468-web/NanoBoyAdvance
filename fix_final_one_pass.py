import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, h_name):
    abs_h = os.path.join(TEAKRA_HDR_ROOT, h_name)
    abs_f = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(abs_h, abs_f).replace('\\', '/')

# Fix melonds core files
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if not file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')): continue
        f_path = os.path.join(root, file)
        with open(f_path, 'rb') as f: data = f.read()
        lines = data.splitlines(keepends=True)
        new_lines = []
        changed = False
        skip_body = False
        for line in lines:
            # 1. Includes
            m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
            if m:
                inc_path = m.group(1).decode('utf-8', 'ignore')
                bname = os.path.basename(inc_path)
                if bname in TARGET_HEADERS:
                    correct = get_rel(f_path, bname)
                    indent = line[:line.find(b'#include')]
                    new_l = indent + b'#include "' + correct.encode() + b'"\n'
                    if new_l.strip() != line.strip():
                        line = new_l
                        changed = True

            # 2. C API removal
            if file == 'teakra_c.cpp' or file == 'teakra_c.h':
                if b'Teakra_GetDspMemory' in line:
                    changed = True
                    if file == 'teakra_c.cpp': skip_body = True
                    continue
                if skip_body:
                    if b'return' in line or b'}' in line:
                        if b'}' in line: skip_body = False
                        continue

            new_lines.append(line)

        if changed:
            with open(f_path, 'wb') as f: f.writelines(new_lines)
            print(f"Fixed {f_path}")

# NDS.cpp (RAM/Version) - clean and apply
nds_path = 'src/core/melonds/NDS.cpp'
with open(nds_path, 'rb') as f: data = f.read()
# Clean duplicate RAM alloc
data = re.sub(rb'(MainRAM = new u8\[MainRAMMaxSize\];\s*SharedWRAM = new u8\[SharedWRAMSize\];\s*ARM7WRAM = new u8\[ARM7WRAMSize\];\s*)+',
              rb'MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];\n', data)
# Clean duplicate dealloc
data = re.sub(rb'(delete\[\] MainRAM;\s*delete\[\] SharedWRAM;\s*delete\[\] ARM7WRAM;\s*)+',
              rb'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n', data)
# Version fallback
if b'#ifndef MELONDS_VERSION' not in data:
    old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    new = b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old, new)
with open(nds_path, 'wb') as f: f.write(data)

# runtime.cpp (GPU/ROM Flow)
rt_path = 'src/core/melonds/runtime.cpp'
with open(rt_path, 'rb') as f: data = f.read()
# Clean duplicate GPU init
data = re.sub(rb'(GPU::InitRenderer\(0\);\s*)+', rb'GPU::InitRenderer(0);\n', data)
# ROM flow
if b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart' in data:
    data = data.replace(b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart', b'runtime.rom_loaded = NDS::LoadCart')
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
with open(rt_path, 'wb') as f: f.write(data)
