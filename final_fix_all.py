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

def process_file(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # Regex to find #include "..." or #include <...>
        # We match based on the presence of one of the target header basenames
        # We look for something that ends in teakra.h, etc.
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path_str = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path_str)
            if basename in TARGET_HEADERS:
                # This is a teakra header.
                correct_rel = get_rel_path(file_path, basename)

                # Check if it's already exactly what we want
                if inc_path_str != correct_rel:
                    # Replace with clean #include "rel_path"
                    # Preserve indentation
                    indent_match = re.search(rb'^(\s*)#include', line)
                    indent = indent_match.group(1) if indent_match else b''
                    new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                    changed = True

        # C API fix: Remove GetDspMemory from teakra_c.cpp/h
        if file_path.endswith('teakra_c.cpp') or file_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                continue # Skip declaration line

        new_lines.append(new_line)

    # Post-process teakra_c.cpp to remove function body
    if file_path.endswith('teakra_c.cpp') and changed:
        final_lines = []
        skip = False
        for l in new_lines:
            # Look for the return statement which was part of the body
            if b'return context->teakra.GetDspMemory()' in l:
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                final_lines.append(l)
        new_lines = final_lines

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

# Fix melonds core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file(os.path.join(root, file))

# Fix NDS.cpp (RAM and Version)
def fix_nds():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    changed = False
    # RAM Allocation
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        if b'ARM7 = new ARMv4();' in data:
            data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
            changed = True
    # RAM Deallocation
    if b'delete[] MainRAM;' not in data:
        if b'NDSCart::DeInit();' in data:
            data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
            changed = True
    # Version
    old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if old_v in data and b'#ifndef MELONDS_VERSION' not in data:
        data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        changed = True

    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        print("Updated NDS.cpp")

fix_nds()

# Fix runtime.cpp (GPU and ROM load)
def fix_runtime():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    changed = False
    # GPU Init
    if b'GPU::InitRenderer(0);' not in data:
        if b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }' in data:
            data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                                b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
            changed = True

    # ROM Load flow
    if b'LoadCart' in data and b'LoadBIOS' in data:
        pos_bios = data.find(b'NDS::LoadBIOS();')
        pos_cart = data.find(b'NDS::LoadCart')
        if pos_bios != -1 and pos_cart != -1 and pos_bios < pos_cart:
            data = data.replace(b'NDS::LoadBIOS();\n', b'')
            data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
            changed = True

    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        print("Updated runtime.cpp")

fix_runtime()
