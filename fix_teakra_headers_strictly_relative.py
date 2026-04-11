import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_ROOT, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')
    return rel

def process_file(file_path):
    if not os.path.isfile(file_path): return
    with open(file_path, 'rb') as f:
        content = f.read()

    lines = content.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        new_line = line
        # Match any #include that ends with one of our target headers
        # Use regex to find the path inside "" or <>
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc_path = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(inc_path)
            if basename in TARGET_HEADERS:
                # Calculate correct relative path
                correct_rel = get_rel(file_path, basename)
                # Overwrite the entire line to be clean
                # Find indentation
                indent = line[:line.find(b'#include')]
                new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                if new_line.strip() != line.strip():
                    changed = True

        new_lines.append(new_line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed includes in {file_path}")

# Run across all relevant files in melonds
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process_file(os.path.join(root, file))

# Specific C API fix (Remove GetDspMemory)
def fix_teakra_c_api():
    cpp_path = 'src/core/melonds/teakra/src/teakra_c.cpp'
    if os.path.exists(cpp_path):
        with open(cpp_path, 'rb') as f:
            lines = f.readlines()
        new_lines = []
        skip = False
        for line in lines:
            if b'Teakra_GetDspMemory' in line:
                skip = True
                continue
            if skip and line.strip() == b'}':
                skip = False
                continue
            if not skip:
                new_lines.append(line)
        with open(cpp_path, 'wb') as f:
            f.writelines(new_lines)
        print("Cleaned teakra_c.cpp")

    h_path = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
    if os.path.exists(h_path):
        with open(h_path, 'rb') as f:
            lines = f.readlines()
        new_lines = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h_path, 'wb') as f:
            f.writelines(new_lines)
        print("Cleaned teakra_c.h")

fix_teakra_c_api()

# Re-apply core logic fixes to ensure they are correct
def fix_core_logic():
    # NDS.cpp
    nds_path = 'src/core/melonds/NDS.cpp'
    with open(nds_path, 'rb') as f:
        data = f.read()

    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')

    if b'delete[] MainRAM;' not in data:
        data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if b'#ifndef MELONDS_VERSION' not in data:
        data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

    with open(nds_path, 'wb') as f:
        f.write(data)
    print("Verified NDS.cpp")

    # runtime.cpp
    rt_path = 'src/core/melonds/runtime.cpp'
    with open(rt_path, 'rb') as f:
        data = f.read()

    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow
    if b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart' in data:
        data = data.replace(b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart', b'runtime.rom_loaded = NDS::LoadCart')
        data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                            b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(rt_path, 'wb') as f:
        f.write(data)
    print("Verified runtime.cpp")

fix_core_logic()
