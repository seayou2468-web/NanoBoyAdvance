import os
import re

def fix_everything():
    # 1. NDS.cpp (RAM Allocation, Deallocation, Version Fallback)
    nds_path = 'src/core/melonds/NDS.cpp'
    if os.path.exists(nds_path):
        with open(nds_path, 'rb') as f: data = f.read()
        # Clean duplicates first
        data = re.sub(rb'\n\s*MainRAM = new u8\[MainRAMMaxSize\].*?ARM7WRAM = new u8\[ARM7WRAMSize\];', b'', data, flags=re.DOTALL)
        data = re.sub(rb'\n\s*delete\[\] MainRAM;.*?delete\[\] ARM7WRAM;', b'', data, flags=re.DOTALL)
        data = re.sub(rb'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n\s*', b'', data)
        # Apply fresh
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
        data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
        old_v = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
        data = data.replace(old_v, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        with open(nds_path, 'wb') as f: f.write(data)
        print("Fixed NDS.cpp")

    # 2. runtime.cpp (GPU Init, ROM Flow)
    rt_path = 'src/core/melonds/runtime.cpp'
    if os.path.exists(rt_path):
        with open(rt_path, 'rb') as f: data = f.read()
        # Clean duplicates
        data = re.sub(rb'\n\s*GPU::InitRenderer\(0\);', b'', data)
        # Apply GPU Init
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        # ROM Flow
        if b'LoadBIOS' in data and b'LoadCart' in data:
            # Remove all LoadBIOS calls first
            data = data.replace(b'NDS::LoadBIOS();\n', b'')
            data = data.replace(b'  NDS::LoadBIOS();', b'')
            # Insert after LoadCart
            data = data.replace(b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);',
                                b'runtime.rom_loaded = NDS::LoadCart(runtime.rom_data.data(), static_cast<uint32_t>(runtime.rom_data.size()), nullptr, 0);\n  NDS::LoadBIOS();')
        with open(rt_path, 'wb') as f: f.write(data)
        print("Fixed runtime.cpp")

    # 3. teakra_c.h (Remove GetDspMemory)
    tch_path = 'src/core/melonds/teakra/include/teakra/teakra_c.h'
    if os.path.exists(tch_path):
        with open(tch_path, 'rb') as f: lines = f.readlines()
        new_lines = [l for l in lines if b'Teakra_GetDspMemory' not in l]
        with open(tch_path, 'wb') as f: f.writelines(new_lines)
        print("Fixed teakra_c.h")

    # 4. Teakra includes and teakra_c.cpp cleanup
    targets = [b'teakra.h', b'teakra_c.h', b'disassembler.h', b'disassembler_c.h']

    # Files at depth 1 (src/*.cpp)
    src1 = 'src/core/melonds/teakra/src/'
    for f in os.listdir(src1):
        if not f.endswith(('.cpp', '.h')): continue
        f_path = os.path.join(src1, f)
        with open(f_path, 'rb') as file: lines = file.readlines()
        new_lines = []
        skip_body = False
        for l in lines:
            if b'Teakra_GetDspMemory' in l:
                if f == 'teakra_c.cpp': skip_body = True
                continue
            if skip_body:
                if b'return' in l or b'}' in l:
                    if b'}' in l: skip_body = False
                    continue
            # Fix include
            m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', l)
            if m:
                path_str = m.group(1)
                bname = os.path.basename(path_str.decode('utf-8', 'ignore'))
                if bname.encode() in targets:
                    indent = l[:l.find(b'#include')]
                    l = indent + b'#include "../include/teakra/' + bname.encode() + b'"\n'
            new_lines.append(l)
        with open(f_path, 'wb') as file: file.writelines(new_lines)
        print(f"Fixed {f_path}")

    # Files at depth 2 (src/*/*.cpp)
    for d in os.listdir(src1):
        d_path = os.path.join(src1, d)
        if os.path.isdir(d_path):
            for f in os.listdir(d_path):
                if not f.endswith(('.cpp', '.h')): continue
                f_path = os.path.join(d_path, f)
                with open(f_path, 'rb') as file: lines = file.readlines()
                new_lines = []
                for l in lines:
                    m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', l)
                    if m:
                        path_str = m.group(1)
                        bname = os.path.basename(path_str.decode('utf-8', 'ignore'))
                        if bname.encode() in targets:
                            indent = l[:l.find(b'#include')]
                            l = indent + b'#include "../../include/teakra/' + bname.encode() + b'"\n'
                    new_lines.append(l)
                with open(f_path, 'wb') as file: file.writelines(new_lines)
                print(f"Fixed {f_path}")

    # DSi_DSP.cpp
    dsp_path = 'src/core/melonds/DSi_DSP.cpp'
    if os.path.exists(dsp_path):
        with open(dsp_path, 'rb') as f: data = f.read()
        data = re.sub(rb'#include\s*[<"]([^>"]*teakra\.h)[>"]', rb'#include "teakra/include/teakra/teakra.h"', data)
        with open(dsp_path, 'wb') as f: f.write(data)
        print("Fixed DSi_DSP.cpp")

fix_everything()
