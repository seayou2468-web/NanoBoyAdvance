import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel_path(from_file, h_name):
    abs_h = os.path.join(TEAKRA_HDR_ROOT, h_name)
    abs_f = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(abs_h, abs_f).replace('\\', '/')
    return rel

# 1. Fix NDS.cpp
def fix_nds():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()

    # Remove all previous additions to RAM alloc/dealloc/version
    data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
    data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
    data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)

    # Re-apply correctly
    data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
    data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    data = data.replace(old_ver, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

    with open(path, 'wb') as f: f.write(data)
    print("Fixed NDS.cpp")

# 2. Fix runtime.cpp
def fix_runtime():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f: data = f.read()

    # Remove previous GPU Init
    data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)

    # Re-apply GPU Init
    data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                        b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # ROM Load flow
    # Reset LoadBIOS state first
    data = data.replace(b'  NDS::LoadBIOS();\n', b'')
    data = data.replace(b'NDS::LoadBIOS();\n', b'')
    # Add LoadBIOS after LoadCart
    data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                        b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(path, 'wb') as f: f.write(data)
    print("Fixed runtime.cpp")

# 3. Fix all includes and C API
def fix_all_includes():
    for root, dirs, files in os.walk('src/core/melonds'):
        for file in files:
            if not file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')): continue
            f_path = os.path.join(root, file)
            with open(f_path, 'rb') as f: content = f.read()

            lines = content.splitlines(keepends=True)
            new_lines = []
            changed = False
            skip_body = False

            for line in lines:
                # Includes
                m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
                if m:
                    inc_path_str = m.group(1).decode('utf-8', 'ignore')
                    bname = os.path.basename(inc_path_str)
                    if bname in TARGET_HEADERS:
                        correct = get_rel_path(f_path, bname)
                        indent_match = re.search(rb'^(\s*)#include', line)
                        indent = indent_match.group(1) if indent_match else b''
                        new_line = indent + b'#include "' + correct.encode() + b'"\n'
                        if new_line.strip() != line.strip():
                            line = new_line
                            changed = True

                # C API removal
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
                with open(f_path, 'wb') as f: f.writelines(new_lines)
                print(f"Fixed {f_path}")

if __name__ == '__main__':
    fix_nds()
    fix_runtime()
    fix_all_includes()
