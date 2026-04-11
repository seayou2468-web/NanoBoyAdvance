import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False

    for line in lines:
        if '#include "../include/teakra/' in line:
            new_lines.append(line.replace('../include/teakra/', 'teakra/'))
            changed = True
        elif '#include "../../include/teakra/' in line:
            new_lines.append(line.replace('../../include/teakra/', 'teakra/'))
            changed = True
        elif '#include "teakra/include/teakra/' in line:
            new_lines.append(line.replace('teakra/include/teakra/', 'teakra/'))
            changed = True
        else:
            new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Normalized {file_path}")

files = [
    'src/core/melonds/teakra/src/disassembler_c.cpp',
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp',
    'src/core/melonds/teakra/src/disassembler.cpp',
    'src/core/melonds/teakra/src/test_verifier/main.cpp',
    'src/core/melonds/teakra/src/parser.cpp',
    'src/core/melonds/teakra/src/coff_reader/main.cpp',
    'src/core/melonds/teakra/src/teakra_c.cpp',
    'src/core/melonds/teakra/src/teakra.cpp',
    'src/core/melonds/DSi_DSP.cpp'
]

for f in files:
    fix_file(f)
