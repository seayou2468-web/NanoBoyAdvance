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

def process_file_includes_and_api(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        content = f.read()

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False
    skip_body = False

    for line in lines:
        new_line = line
        # 1. Fix includes
        # Match #include <...> or #include "..."
        # We look for any include that ends with one of our target headers
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path_str = m.group(1).decode('utf-8', 'ignore')
            # Extract filename from path
            basename = os.path.basename(inc_path_str)
            if basename in TARGET_HEADERS:
                correct_rel = get_rel_path(file_path, basename)
                # Construct exactly: #include "relative/path/to/header.h"
                # Preserve indentation
                indent_match = re.search(rb'^(\s*)#include', line)
                indent = indent_match.group(1) if indent_match else b''
                new_l = indent + b'#include "' + correct_rel.encode() + b'"\n'
                if new_l.strip() != line.strip():
                    new_line = new_l
                    changed = True

        # 2. C API fix (Remove Teakra_GetDspMemory)
        if file_path.endswith('teakra_c.cpp') or file_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                if file_path.endswith('teakra_c.cpp'): skip_body = True
                continue
            if skip_body:
                if b'return' in line or b'}' in line:
                    if b'}' in line: skip_body = False
                    continue

        new_lines.append(new_line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

# Fix melonds core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file_includes_and_api(os.path.join(root, file))

# Fix NDS.cpp (RAM and Version)
def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # Reset to known base by removing previous additions
    data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
    data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
    data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)

    # Re-apply correctly
    # RAM Allocation
    data = data.replace(b'ARM9 = new ARMv5();\n    ARM7 = new ARMv4();', b'ARM9 = new ARMv5();\n    ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
    # RAM Deallocation
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
    # Version Fallback
    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

    with open(path, 'wb') as f:
        f.write(data)
    print("Updated NDS.cpp")

fix_nds_cpp()

# Fix runtime.cpp (GPU and ROM load)
def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # Clean previous GPU Init
    data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)
    # Apply GPU Init
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow - Ensure LoadBIOS is AFTER LoadCart
    # Remove all NDS::LoadBIOS(); in the file (there should be only one in LoadROMFromMemory)
    data = data.replace(b'  NDS::LoadBIOS();\n', b'')
    data = data.replace(b'NDS::LoadBIOS();\n', b'')
    # Apply correctly after LoadCart
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(path, 'wb') as f:
        f.write(data)
    print("Updated runtime.cpp")

fix_runtime_cpp()

# Final check: Fix Config/Project.plist include paths just in case
def fix_plist():
    path = 'Config/Project.plist'
    if not os.path.exists(path): return
    with open(path, 'r') as f:
        content = f.read()
    if '-Isrc/core/melonds' not in content:
        content = content.replace('<string>-fobjc-arc</string>',
                                  '<string>-fobjc-arc</string>\n\t\t<string>-Isrc/core/melonds</string>\n\t\t<string>-Isrc/core/melonds/teakra/include</string>')
        with open(path, 'w') as f:
            f.write(content)
        print("Updated Config/Project.plist")

fix_plist()
