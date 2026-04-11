import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel_path(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_ROOT, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')
    return rel

def process_file_includes_and_api(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        content = f.read()

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False
    skip_body = False

    for line in lines:
        # 1. Fix includes
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path_str = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path_str)
            if basename in TARGET_HEADERS:
                correct_rel = get_rel_path(file_path, basename)
                indent_match = re.search(rb'^(\s*)#include', line)
                indent = indent_match.group(1) if indent_match else b''
                new_l = indent + b'#include "' + correct_rel.encode() + b'"\n'
                if new_l.strip() != line.strip():
                    line = new_l
                    changed = True

        # 2. C API fix
        if file_path.endswith('teakra_c.cpp') or file_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                if file_path.endswith('teakra_c.cpp'): skip_body = True
                continue
            if skip_body:
                if b'return' in line or b'}' in line:
                    if b'}' in line: skip_body = False
                    continue

        new_lines.append(line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

# Fix melonds core files
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file_includes_and_api(os.path.join(root, file))

# Definitive NDS.cpp
def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    with open(path, 'rb') as f: data = f.read()

    # 1. Version fallback
    if b'#ifndef MELONDS_VERSION' not in data:
        old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
        new = b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;'
        data = data.replace(old, new)

    # 2. RAM Alloc in Init
    # First remove any existing ones to avoid mess
    data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
    # Apply clean
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')

    # 3. RAM Dealloc in DeInit
    data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

    with open(path, 'wb') as f: f.write(data)
    print("Cleaned and updated NDS.cpp")

fix_nds_cpp()

# Definitive runtime.cpp
def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    with open(path, 'rb') as f: data = f.read()

    # GPU Init cleanup
    data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow cleanup
    # Remove all NDS::LoadBIOS();
    data = data.replace(b'  NDS::LoadBIOS();\n', b'')
    data = data.replace(b'NDS::LoadBIOS();\n', b'')
    # Apply correctly
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(path, 'wb') as f: f.write(data)
    print("Cleaned and updated runtime.cpp")

fix_runtime_cpp()
