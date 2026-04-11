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

def fix_all_includes_and_c_api():
    for root, dirs, files in os.walk('src/core/melonds'):
        for file in files:
            if not file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
                continue

            file_path = os.path.join(root, file)
            with open(file_path, 'rb') as f:
                content = f.read()

            lines = content.splitlines(keepends=True)
            new_lines = []
            changed = False
            skip_body = False

            for line in lines:
                # 1. Includes
                m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
                if m:
                    inc_path_str = m.group(1).decode('utf-8', 'ignore')
                    basename = os.path.basename(inc_path_str)
                    if basename in TARGET_HEADERS:
                        correct_rel = get_rel_path(file_path, basename)
                        indent_match = re.search(rb'^(\s*)#include', line)
                        indent = indent_match.group(1) if indent_match else b''
                        new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                        if new_line.strip() != line.strip():
                            line = new_line
                            changed = True

                # 2. C API fix
                if file == 'teakra_c.cpp' or file == 'teakra_c.h':
                    if b'Teakra_GetDspMemory' in line:
                        changed = True
                        if file == 'teakra_c.cpp': skip_body = True
                        continue
                    if skip_body:
                        if b'return' in line or b'}' in line:
                            if b'}' in line: skip_body = False
                            continue

                new_lines.append(line)

            if changed:
                with open(file_path, 'wb') as f:
                    f.writelines(new_lines)
                print(f"Fixed includes/API in {file_path}")

def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # Clean up previous attempts to avoid duplicates
    data = re.sub(rb'\s*MainRAM = new u8\[MainRAMMaxSize\];\s*SharedWRAM = new u8\[SharedWRAMSize\];\s*ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data)
    data = re.sub(rb'\s*delete\[\] MainRAM;\s*delete\[\] SharedWRAM;\s*delete\[\] ARM7WRAM;', b'', data)
    data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)

    # Apply fresh
    # RAM Allocation
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
    # RAM Deallocation
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
    # Version Fallback
    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

    with open(path, 'wb') as f:
        f.write(data)
    print("Cleaned and updated NDS.cpp")

def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # Clean duplicates
    data = re.sub(rb'\s*GPU::InitRenderer\(0\);', b'', data)

    # Apply GPU Init
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow - Ensure LoadBIOS is AFTER LoadCart
    # First, make sure we only have one LoadBIOS in the function
    # Find LoadROMFromMemory start
    match = re.search(rb'bool LoadROMFromMemory\(.*?\)\s*\{', data, re.DOTALL)
    if match:
        start = match.end()
        # Find next } (end of function) - assuming it is simple
        end = data.find(b'\n}', start)
        func_body = data[start:end]

        # Remove all NDS::LoadBIOS();
        func_body = func_body.replace(b'NDS::LoadBIOS();\n', b'')
        func_body = func_body.replace(b'  NDS::LoadBIOS();', b'')

        # Insert after LoadCart
        func_body = func_body.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                      b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

        data = data[:start] + func_body + data[end:]

    with open(path, 'wb') as f:
        f.write(data)
    print("Cleaned and updated runtime.cpp")

if __name__ == '__main__':
    fix_all_includes_and_c_api()
    fix_nds_cpp()
    fix_runtime_cpp()
