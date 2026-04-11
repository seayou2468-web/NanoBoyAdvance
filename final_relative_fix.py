import os
import re

REPO_ROOT = os.path.abspath('.')
# The headers reside in:
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, h_name):
    h_abs = os.path.join(TEAKRA_HDR_BASE, h_name)
    f_dir = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(h_abs, f_dir).replace('\\', '/')
    return rel

def process_file(f_path):
    if not os.path.isfile(f_path): return
    with open(f_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False
    skip_body = False

    for line in lines:
        new_line = line
        # 1. Match #include
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path_str = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path_str)
            if basename in TARGET_HEADERS:
                correct_rel = get_rel(f_path, basename)
                if inc_path_str != correct_rel:
                    # Replace with clean #include "rel_path"
                    indent_match = re.search(rb'^(\s*)#include', line)
                    indent = indent_match.group(1) if indent_match else b''
                    new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                    changed = True

        # 2. C API fix
        if f_path.endswith('teakra_c.cpp') or f_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                if f_path.endswith('teakra_c.cpp'): skip_body = True
                continue
            if skip_body:
                if b'return' in line or b'}' in line:
                    if b'}' in line: skip_body = False
                    continue

        new_lines.append(new_line)

    if changed:
        with open(f_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {f_path}")

# Run for all files in core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file(os.path.join(root, file))

# Core Logic Fixes
def fix_nds():
    path = 'src/core/melonds/NDS.cpp'
    with open(path, 'rb') as f: data = f.read()
    # Clean duplicates
    data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
    data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
    data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)
    # Apply
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
    old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
    with open(path, 'wb') as f: f.write(data)

def fix_runtime():
    path = 'src/core/melonds/runtime.cpp'
    with open(path, 'rb') as f: data = f.read()
    data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
    data = data.replace(b'NDS::LoadBIOS();\n', b'')
    data = data.replace(b'  NDS::LoadBIOS();', b'')
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
    with open(path, 'wb') as f: f.write(data)

fix_nds()
fix_runtime()
