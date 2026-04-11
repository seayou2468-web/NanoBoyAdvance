import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel_path(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')
    return rel

def fix_file(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        content = f.read()

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False
    skip_body = False

    # Check if we are in teakra_c.cpp
    is_teakra_c_cpp = file_path.endswith('teakra_c.cpp')
    is_teakra_c_h = file_path.endswith('teakra_c.h')

    for line in lines:
        # 1. Fix includes
        # Match #include <...> or #include "..."
        # We look for any include that ends with one of our target headers
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

        # 2. C API fix: Remove Teakra_GetDspMemory
        if is_teakra_c_cpp or is_teakra_c_h:
            if b'Teakra_GetDspMemory' in line:
                changed = True
                if is_teakra_c_cpp: skip_body = True
                continue
            if skip_body:
                # In teakra_c.cpp, the body is:
                # return context->teakra.GetDspMemory().data();
                # }
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
            fix_file(os.path.join(root, file))

# Fix NDS.cpp (RAM and Version)
def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()

    # Clean previous additions
    data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
    data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
    data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)

    # Re-apply RAM Allocation in Init()
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')

    # Re-apply RAM Deallocation in DeInit()
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

    # Re-apply Version Fallback
    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

    with open(path, 'wb') as f: f.write(data)
    print("Updated NDS.cpp")

fix_nds_cpp()

# Fix runtime.cpp (GPU and ROM load)
def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()

    # Clean previous GPU Init
    data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)

    # Re-apply GPU Init
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow - Move LoadBIOS after LoadCart
    # Reset first
    data = data.replace(b'  NDS::LoadBIOS();\n', b'')
    data = data.replace(b'NDS::LoadBIOS();\n', b'')
    # Apply correctly
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(path, 'wb') as f: f.write(data)
    print("Updated runtime.cpp")

fix_runtime_cpp()
