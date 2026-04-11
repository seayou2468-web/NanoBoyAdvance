import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_INCLUDE_DIR = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel_path(from_file, header_name):
    header_abs = os.path.join(TEAKRA_INCLUDE_DIR, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(header_abs, file_dir_abs).replace('\\', '/')
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
        # We search for any include that ends with one of our target header names
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            include_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(include_path)
            if basename in TARGET_HEADERS:
                correct_rel = get_rel_path(file_path, basename)
                # Overwrite the entire line to ensure clean format
                new_line = b'#include "' + correct_rel.encode() + b'"\n'
                if new_line.strip() != line.strip():
                    changed = True

        # C API fix (Remove GetDspMemory)
        if file_path.endswith('teakra_c.cpp') or file_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                continue # Skip this line

        new_lines.append(new_line)

    # Post-process teakra_c.cpp for function body
    if file_path.endswith('teakra_c.cpp') and changed:
        final_lines = []
        skip = False
        for l in new_lines:
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
    # Version fallback
    old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if old_v in data and b'#ifndef MELONDS_VERSION' not in data:
        data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        changed = True

    # RAM Allocation in Init()
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        # Find the line "ARM7 = new ARMv4();" and insert after it
        if b'ARM7 = new ARMv4();' in data:
            data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
            changed = True

    # RAM Deallocation in DeInit()
    if b'delete[] MainRAM;' not in data:
        if b'NDSCart::DeInit();' in data:
            data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
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
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        changed = True

    # ROM Flow: Move LoadBIOS after LoadCart
    if b'LoadBIOS' in data and b'LoadCart' in data:
        # Check order
        pos_bios = data.find(b'NDS::LoadBIOS();')
        pos_cart = data.find(b'NDS::LoadCart')
        if pos_bios != -1 and pos_cart != -1 and pos_bios < pos_cart:
            # Swap or move
            data = data.replace(b'NDS::LoadBIOS();\n', b'')
            data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
            changed = True

    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        print("Updated runtime.cpp")

fix_runtime()
