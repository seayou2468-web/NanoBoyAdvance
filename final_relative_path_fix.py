import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')

def get_rel(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

TARGETS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def process_file(f_path):
    with open(f_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # Match any #include that might be a teakra header
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path)
            if basename in TARGETS:
                # Correct it to a relative path
                rel = get_rel(f_path, basename)
                # We overwrite the whole line to be clean
                new_line = b'#include "' + rel.encode() + b'"\n'
                if new_line.strip() != line.strip():
                    changed = True

        # C API Fixes
        if f_path.endswith('teakra_c.cpp'):
            if b'uint8_t* Teakra_GetDspMemory' in line:
                changed = True
                continue

        if f_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                continue

        new_lines.append(new_line)

    if f_path.endswith('teakra_c.cpp') and changed:
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
        with open(f_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {f_path}")

# Run across all relevant files
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file(os.path.join(root, file))

# RAM Allocation and Version fallback in NDS.cpp
nds_path = 'src/core/melonds/NDS.cpp'
with open(nds_path, 'rb') as f:
    data = f.read()
nds_changed = False
if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
    nds_changed = True
if b'delete[] MainRAM;' not in data:
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
    nds_changed = True
old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
if old_ver in data and b'#ifndef MELONDS_VERSION' not in data:
    data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
    nds_changed = True
if nds_changed:
    with open(nds_path, 'wb') as f:
        f.write(data)

# runtime.cpp sequence
rt_path = 'src/core/melonds/runtime.cpp'
with open(rt_path, 'rb') as f:
    data = f.read()
rt_changed = False
if b'GPU::InitRenderer(0);' not in data:
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
    rt_changed = True
if b'LoadCart' in data and b'LoadBIOS' in data:
    if data.find(b'NDS::LoadBIOS();') < data.find(b'NDS::LoadCart'):
        data = data.replace(b'NDS::LoadBIOS();\n', b'')
        data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                            b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
        rt_changed = True
if rt_changed:
    with open(rt_path, 'wb') as f:
        f.write(data)
