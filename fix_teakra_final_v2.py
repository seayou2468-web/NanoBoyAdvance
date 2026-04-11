import os

# 1. Normalize include paths to use relative paths correctly
def fix_includes(file_path):
    if not os.path.exists(file_path):
        return
    with open(file_path, 'r') as f:
        lines = f.readlines()

    dir_path = os.path.dirname(file_path)
    if dir_path == 'src/core/melonds/teakra/src':
        rel_prefix = '../include/'
    elif dir_path.startswith('src/core/melonds/teakra/src/'):
        rel_prefix = '../../include/'
    else:
        return

    new_lines = []
    changed = False
    for line in lines:
        if '#include "' in line and 'teakra/' in line:
            # Normalize to use rel_prefix
            if '../include/teakra/' in line:
                new_line = line.replace('../include/teakra/', rel_prefix + 'teakra/')
            elif '../../include/teakra/' in line:
                new_line = line.replace('../../include/teakra/', rel_prefix + 'teakra/')
            else:
                new_line = line

            if new_line != line:
                new_lines.append(new_line)
                changed = True
                continue
        new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Fixed includes in {file_path}")

# 2. Remove non-existent Teakra_GetDspMemory from C API
def fix_teakra_c(file_path, header_path):
    # Fix src/core/melonds/teakra/src/teakra_c.cpp
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            lines = f.readlines()
        new_lines = []
        skip = False
        for line in lines:
            if 'uint8_t* Teakra_GetDspMemory(TeakraContext* context) {' in line:
                skip = True
                continue
            if skip and line.strip() == '}':
                skip = False
                continue
            if not skip:
                new_lines.append(line)
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Removed GetDspMemory from {file_path}")

    # Fix src/core/melonds/teakra/include/teakra/teakra_c.h
    if os.path.exists(header_path):
        with open(header_path, 'r') as f:
            lines = f.readlines()
        new_lines = [l for l in lines if 'Teakra_GetDspMemory' not in l]
        with open(header_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Removed GetDspMemory from {header_path}")

files = [
    'src/core/melonds/teakra/src/disassembler_c.cpp',
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp',
    'src/core/melonds/teakra/src/disassembler.cpp',
    'src/core/melonds/teakra/src/test_verifier/main.cpp',
    'src/core/melonds/teakra/src/parser.cpp',
    'src/core/melonds/teakra/src/coff_reader/main.cpp',
    'src/core/melonds/teakra/src/teakra_c.cpp',
    'src/core/melonds/teakra/src/teakra.cpp'
]

for f in files:
    fix_includes(f)

fix_teakra_c('src/core/melonds/teakra/src/teakra_c.cpp', 'src/core/melonds/teakra/include/teakra/teakra_c.h')
