import os
import re

# Repo root
ROOT = os.path.abspath('.')

# Teakra Header directory
HDR_DIR = os.path.join(ROOT, 'src/core/melonds/teakra/include/teakra')

def get_relative(from_file, header_name):
    # Absolute path to header
    h_abs = os.path.join(HDR_DIR, header_name)
    # Absolute path to directory containing from_file
    f_dir = os.path.abspath(os.path.dirname(from_file))
    # Relative path
    rel = os.path.relpath(h_abs, f_dir).replace('\\', '/')
    return rel

TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def fix_file(path):
    with open(path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    # Context for teakra_c.cpp body removal
    skip_body = False

    for line in lines:
        # 1. Includes
        # Look for #include "..." or #include <...>
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path)
            if basename in TARGET_HEADERS:
                # It's a teakra header. Force relative path.
                rel_path = get_relative(path, basename)

                # Check indentation
                indent_match = re.search(rb'^(\s*)#include', line)
                indent = indent_match.group(1) if indent_match else b''

                new_line = indent + b'#include "' + rel_path.encode() + b'"\n'
                if new_line.strip() != line.strip():
                    line = new_line
                    changed = True

        # 2. C API Cleanup (Teakra_GetDspMemory)
        if path.endswith('teakra_c.cpp') or path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                if path.endswith('teakra_c.cpp'):
                    skip_body = True
                continue
            if skip_body:
                # In teakra_c.cpp:
                # return context->teakra.GetDspMemory().data();
                # }
                if b'return' in line or b'}' in line:
                    if b'}' in line: skip_body = False
                    continue

        new_lines.append(line)

    if changed:
        with open(path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {path}")

# Walk core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            fix_file(os.path.join(root, file))

# Fix NDS.cpp RAM/Version (Idempotent)
def fix_nds():
    p = 'src/core/melonds/NDS.cpp'
    with open(p, 'rb') as f: data = f.read()
    changed = False
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
        changed = True
    if b'delete[] MainRAM;' not in data:
        data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
        changed = True
    if b'#ifndef MELONDS_VERSION' not in data:
        old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
        data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        changed = True
    if changed:
        with open(p, 'wb') as f: f.write(data)
        print("Updated NDS.cpp")

# Fix runtime.cpp (GPU/ROM Flow)
def fix_runtime():
    p = 'src/core/melonds/runtime.cpp'
    with open(p, 'rb') as f: data = f.read()
    changed = False
    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        changed = True
    if b'LoadBIOS' in data and b'LoadCart' in data:
        if data.find(b'NDS::LoadBIOS();') < data.find(b'NDS::LoadCart'):
            data = data.replace(b'NDS::LoadBIOS();\n', b'')
            data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
            changed = True
    if changed:
        with open(p, 'wb') as f: f.write(data)
        print("Updated runtime.cpp")

fix_nds()
fix_runtime()
