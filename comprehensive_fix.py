import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGET_HEADERS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_correct_relative_path(from_file, header_name):
    header_abs = os.path.join(TEAKRA_HDR_BASE, header_name)
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    rel = os.path.relpath(header_abs, file_dir_abs).replace('\\', '/')
    # Ensure no double slashes and normalized
    rel = os.path.normpath(rel).replace('\\', '/')
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

            for line in lines:
                new_line = line
                # 1. Match #include
                m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
                if m:
                    inc_path_str = m.group(1).decode('utf-8', 'ignore')
                    basename = os.path.basename(inc_path_str)
                    if basename in TARGET_HEADERS:
                        correct_rel = get_correct_relative_path(file_path, basename)
                        if inc_path_str != correct_rel:
                            # Replace the match with a clean #include "rel_path"
                            # We replace the whole thing including #include to be safe
                            new_line = b'#include "' + correct_rel.encode() + b'"' + line[line.find(m.group(0)) + len(m.group(0)):]
                            if b'\n' not in new_line: new_line += b'\n'
                            changed = True

                # 2. C API fix: Remove Teakra_GetDspMemory line
                if file.endswith('teakra_c.cpp') or file.endswith('teakra_c.h'):
                    if b'Teakra_GetDspMemory' in line:
                        changed = True
                        continue # Skip this line

                new_lines.append(new_line)

            # Post-process teakra_c.cpp for function body
            if file.endswith('teakra_c.cpp') and changed:
                final_lines = []
                skip = False
                for l in new_lines:
                    # We already removed the declaration line, but we might have missed the body if it was multi-line
                    # Actually the previous loop removed any line containing Teakra_GetDspMemory
                    # If the body started on the next line:
                    if skip:
                        if l.strip() == b'}':
                            skip = False
                        continue
                    # Re-check if we missed a body. In teakra_c.cpp it was:
                    # uint8_t* Teakra_GetDspMemory(TeakraContext* context) {
                    #     return context->teakra.GetDspMemory().data();
                    # }
                    # The first line is gone. If the next line was the return:
                    if b'return context->teakra.GetDspMemory()' in l:
                        skip = True
                        continue
                    final_lines.append(l)
                new_lines = final_lines

            if changed:
                with open(file_path, 'wb') as f:
                    f.writelines(new_lines)
                print(f"Fixed {file_path}")

def fix_nds_cpp():
    path = 'src/core/melonds/NDS.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    changed = False
    # 1. Version fallback
    v_old = b'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
    if v_old in data and b'#ifndef MELONDS_VERSION' not in data:
        data = data.replace(v_old, b'#ifndef MELONDS_VERSION\n#define MELONDS_VERSION "unknown"\n#endif\n        static char const emuID[16] = "melonDS " MELONDS_VERSION;')
        changed = True

    # 2. RAM Allocation
    if b'MainRAM = new u8[MainRAMMaxSize];' not in data:
        data = data.replace(b'ARM7 = new ARMv4();', b'ARM7 = new ARMv4();\n\n    MainRAM = new u8[MainRAMMaxSize];\n    SharedWRAM = new u8[SharedWRAMSize];\n    ARM7WRAM = new u8[ARM7WRAMSize];')
        changed = True

    # 3. RAM Deallocation
    if b'delete[] MainRAM;' not in data:
        data = data.replace(b'NDSCart::DeInit();', b'delete[] MainRAM;\n    delete[] SharedWRAM;\n    delete[] ARM7WRAM;\n\n    NDSCart::DeInit();')
        changed = True

    if changed:
        with open(path, 'wb') as f:
            f.write(data)
        print("Updated NDS.cpp")

def fix_runtime_cpp():
    path = 'src/core/melonds/runtime.cpp'
    if not os.path.exists(path): return
    with open(path, 'rb') as f:
        data = f.read()

    changed = False
    # 1. GPU Init
    if b'GPU::InitRenderer(0);' not in data:
        data = data.replace(b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }',
                            b'if (!NDS::Init()) {\n      Platform::DeInit();\n      return nullptr;\n    }\n    GPU::InitRenderer(0);')
        changed = True

    # 2. ROM Load Flow
    if b'LoadCart' in data and b'LoadBIOS' in data:
        # Check if LoadBIOS is BEFORE LoadCart
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

if __name__ == '__main__':
    fix_all_includes_and_c_api()
    fix_nds_cpp()
    fix_runtime_cpp()
