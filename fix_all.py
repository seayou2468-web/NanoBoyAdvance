import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

def process_file(f_path):
    if not os.path.isfile(f_path): return
    with open(f_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # 1. Fix includes
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path)
            if basename in TARGET_HEADERS:
                correct_rel = get_rel(f_path, basename)
                if inc_path != correct_rel:
                    # Construct the new line preserving indentation
                    match_obj = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
                    start = match_obj.start(1)
                    end = match_obj.end(1)
                    # We also want to replace <> with ""
                    new_line = line[:start-1] + b'"' + correct_rel.encode() + b'"' + line[end+1:]
                    changed = True

        # 2. C API fix: Remove Teakra_GetDspMemory
        if f_path.endswith('teakra_c.cpp') or f_path.endswith('teakra_c.h'):
            if b'Teakra_GetDspMemory' in line:
                changed = True
                continue

        new_lines.append(new_line)

    # Function body removal for teakra_c.cpp
    if f_path.endswith('teakra_c.cpp') and changed:
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
        with open(f_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {f_path}")

# Core Logic Fixes
def fix_nds():
    path = 'src/core/melonds/NDS.cpp'
    with open(path, 'rb') as f:
        data = f.read()
    changed = False
    # RAM Allocation
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
        changed = True
    if b'delete[] MainRAM;' not in data:
        data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
        changed = True
    # Version
    old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if b'#ifndef MELONDS_VERSION' not in data:
        data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        changed = True
    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        print("Updated NDS.cpp")

def fix_runtime():
    path = 'src/core/melonds/runtime.cpp'
    with open(path, 'rb') as f:
        data = f.read()
    changed = False
    # GPU Init
    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        changed = True
    # ROM Load flow
    if b'LoadBIOS();' in data and b'LoadCart' in data:
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

# Execution
if __name__ == '__main__':
    for root, dirs, files in os.walk('src/core/melonds'):
        for file in files:
            if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
                process_file(os.path.join(root, file))
    fix_nds()
    fix_runtime()
