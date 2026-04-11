import os

def replace_in_file(path, search, replace):
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()
    if search in data:
        data = data.replace(search, replace)
        with open(path, 'wb') as f:
            f.write(data)
        print(f"Fixed {path}")

# 1. RAM Allocation in NDS.cpp
replace_in_file('src/core/melonds/NDS.cpp',
    b'ARM7 = new ARMv4();',
    b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')

# 2. RAM Deallocation in NDS.cpp
# Be careful not to duplicate if already there
with open('src/core/melonds/NDS.cpp', 'rb') as f:
    nds_data = f.read()
if b'delete[] MainRAM;' not in nds_data:
    replace_in_file('src/core/melonds/NDS.cpp',
        b'NDSCart::DeInit();',
        b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

# 3. Version macro fallback in NDS.cpp
old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
if b'#ifndef MELONDS_VERSION' not in nds_data:
    replace_in_file('src/core/melonds/NDS.cpp',
        old_ver,
        b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')

# 4. GPU Init and ROM Load flow in runtime.cpp
replace_in_file('src/core/melonds/runtime.cpp',
    b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
    b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

with open('src/core/melonds/runtime.cpp', 'rb') as f:
    rt_data = f.read()
# Ensure LoadBIOS is after LoadCart
if b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart' in rt_data:
    rt_data = rt_data.replace(b'NDS::LoadBIOS();\n  runtime.rom_loaded = NDS::LoadCart', b'runtime.rom_loaded = NDS::LoadCart')
    rt_data = rt_data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                              b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
    with open('src/core/melonds/runtime.cpp', 'wb') as f:
        f.write(rt_data)
    print("Fixed runtime.cpp load flow")

# 5. C API fix: Completely remove Teakra_GetDspMemory
def remove_get_dsp_memory():
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
        print("Removed GetDspMemory from teakra_c.cpp")

    h_path = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
    if os.path.exists(h_path):
        with open(h_path, 'rb') as f:
            lines = f.readlines()
        new_lines = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h_path, 'wb') as f:
            f.writelines(new_lines)
        print("Removed GetDspMemory from teakra_c.h")

remove_get_dsp_memory()

# 6. Relative includes for ALL teakra files
def fix_teakra_includes():
    # Base include dir is src/core/melonds/teakra/include/teakra/
    # We want to replace any include that refers to our 4 headers with a strictly relative path.
    targets = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    for root, dirs, files in os.walk('src/core/melonds'):
        for file in files:
            if not file.endswith(('.cpp', '.h', '.hpp', '.c')): continue
            f_path = os.path.join(root, file)
            with open(f_path, 'rb') as f:
                content = f.read()

            lines = content.splitlines(keepends=True)
            new_lines = []
            changed = False

            for line in lines:
                new_line = line
                if b'#include' in line:
                    import re
                    m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
                    if m:
                        inc_path = m.group(1).decode('utf-8', 'ignore')
                        basename = os.path.basename(inc_path)
                        if basename in targets:
                            # Calculate strict relative path
                            # Target header is at src/core/melonds/teakra/include/teakra/<basename>
                            hdr_abs = os.path.abspath(os.path.join('src/core/melonds/teakra/include/teakra', basename))
                            file_dir_abs = os.path.abspath(os.path.dirname(f_path))
                            rel = os.path.relpath(hdr_abs, file_dir_abs).replace('\\', '/')

                            new_line = b'#include "' + rel.encode() + b'"\n'
                            if new_line.strip() != line.strip():
                                changed = True
                new_lines.append(new_line)

            if changed:
                with open(f_path, 'wb') as f:
                    f.writelines(new_lines)
                print(f"Fixed includes in {f_path}")

fix_teakra_includes()
