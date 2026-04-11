import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_clean_relative_path(from_file, header_name):
    header_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(header_abs, file_dir_abs).replace('\\', '/')
    return rel

def process_teakra_includes(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        content = f.read()

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # Match #include <...> or #include "..."
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path_str = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path_str)
            if basename in TARGET_HEADERS:
                # This is a target Teakra header. Force it to be the correct relative path.
                correct_rel = get_clean_relative_path(file_path, basename)

                # Check if it needs replacement
                if inc_path_str != correct_rel:
                    # Construct the new line preserving indentation
                    # We'll use a simple approach: find the original include part and replace the path
                    # But re.sub is better if we are careful.
                    # Or just:
                    indent_match = re.search(rb'^(\s*)#include', line)
                    indent = indent_match.group(1) if indent_match else b''
                    new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                    changed = True
        new_lines.append(new_line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed includes in {file_path}")

# 1. Fix all includes in melonds core
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_teakra_includes(os.path.join(root, file))

# 2. Fix NDS.cpp (RAM and Version)
def fix_nds():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()
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
        with open(path, 'wb') as f: f.write(data)
        print("Fixed NDS.cpp")

fix_nds()

# 3. Fix runtime.cpp (GPU and ROM load flow)
def fix_runtime():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()
    changed = False
    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        changed = True
    if b'LoadCart' in data and b'LoadBIOS' in data:
        pos_bios = data.find(b'NDS::LoadBIOS();')
        pos_cart = data.find(b'NDS::LoadCart')
        if pos_bios != -1 and pos_cart != -1 and pos_bios < pos_cart:
            data = data.replace(b'NDS::LoadBIOS();\n', b'')
            data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
            changed = True
    if changed:
        with open(path, 'wb') as f: f.write(data)
        print("Fixed runtime.cpp")

fix_runtime()

# 4. Remove Teakra_GetDspMemory
def fix_c_api():
    cpp = 'src/core/melonds/teakra/src/teakra_c.cpp'
    h = 'src/core/melonds/teakra/include/teakra/teakra_c.h'

    if os.path.exists(cpp):
        with open(cpp, 'rb') as f: lines = f.readlines()
        new_lines = []
        skip = False
        for l in lines:
            if b'Teakra_GetDspMemory' in l:
                skip = True
                continue
            if skip:
                if b'return' in l or b'}' in l:
                    if b'}' in l: skip = False
                    continue
            new_lines.append(l)
        with open(cpp, 'wb') as f: f.writelines(new_lines)
        print("Cleaned teakra_c.cpp")

    if os.path.exists(h):
        with open(h, 'rb') as f: lines = f.readlines()
        new_h = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h, 'wb') as f: f.writelines(new_h)
        print("Cleaned teakra_c.h")

fix_c_api()
