import os
import re

def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # 1. RAM Allocation
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')

    # 2. RAM Deallocation
    if b'delete[] MainRAM;' not in data:
        # Avoid duplicate deallocs if already present
        if b'delete[] MainRAM;' not in data:
             data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')

    # 3. Version Macro
    old_ver = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if old_ver in data and b'#ifndef MELONDS_VERSION' not in data:
        rep_ver = b'''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;'''
        data = data.replace(old_ver, rep_ver)

    with open(path, 'wb') as f:
        f.write(data)

def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    # 1. GPU Init
    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')

    # 2. ROM Load Flow
    if b'NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();' not in data:
        # Remove any existing LoadBIOS
        data = data.replace(b'NDS::LoadBIOS();\n', b'')
        # Add it after LoadCart
        data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                            b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')

    with open(path, 'wb') as f:
        f.write(data)

def fix_teakra_c_api():
    cpp = 'src/core/melonds/teakra/src/teakra_c.cpp'
    h = 'src/core/melonds/teakra/include/teakra/teakra_c.h'

    if os.path.exists(cpp):
        with open(cpp, 'rb') as f:
            lines = f.readlines()
        new_lines = []
        skip = False
        for l in lines:
            if b'uint8_t* Teakra_GetDspMemory' in l:
                skip = True
                continue
            if skip and l.strip() == b'}':
                skip = False
                continue
            if not skip:
                new_lines.append(l)
        with open(cpp, 'wb') as f:
            f.writelines(new_lines)

    if os.path.exists(h):
        with open(h, 'rb') as f:
            lines = f.readlines()
        new_lines = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(h, 'wb') as f:
            f.writelines(new_lines)

def fix_all_includes():
    # Correct relative paths to teakra headers
    # Headers are in src/core/melonds/teakra/include/teakra/

    mapping = {
        'src/core/melonds/teakra/src/teakra.cpp': ['../include/teakra/teakra.h'],
        'src/core/melonds/teakra/src/teakra_c.cpp': ['../include/teakra/teakra.h', '../include/teakra/teakra_c.h'],
        'src/core/melonds/teakra/src/disassembler.cpp': ['../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/disassembler_c.cpp': ['../include/teakra/disassembler.h', '../include/teakra/disassembler_c.h'],
        'src/core/melonds/teakra/src/parser.cpp': ['../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/dsp1_reader/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/test_verifier/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/coff_reader/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/mod_test_generator/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/test_generator/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/step2_test_generator/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/teakra/src/makedsp1/main.cpp': ['../../include/teakra/disassembler.h'],
        'src/core/melonds/DSi_DSP.cpp': ['teakra/include/teakra/teakra.h']
    }

    headers = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

    for f_path, correct_rels in mapping.items():
        if not os.path.exists(f_path): continue
        with open(f_path, 'rb') as f:
            data = f.read()

        changed = False
        for rel in correct_rels:
            basename = os.path.basename(rel)
            # Match any #include that ends with this header name
            pattern = rb'#include\s*[<"]([^>"]*' + basename.encode() + rb')[>"]'

            def rep(m):
                return b'#include "' + rel.encode() + b'"'

            new_data = re.sub(pattern, rep, data)
            if new_data != data:
                data = new_data
                changed = True

        if changed:
            with open(f_path, 'wb') as f:
                f.write(data)
            print(f"Fixed includes in {f_path}")

fix_nds_cpp()
fix_runtime_cpp()
fix_teakra_c_api()
fix_all_includes()
