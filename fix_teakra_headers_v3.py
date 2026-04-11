import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False

    dir_path = os.path.dirname(file_path)
    if 'src/core/melonds/teakra/src' in file_path:
        if dir_path == 'src/core/melonds/teakra/src':
             rel_prefix = '../include/'
        else:
             rel_prefix = '../../include/'
    else:
        return

    for line in lines:
        if '#include <teakra/teakra.h>' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/teakra.h"\n')
            changed = True
        elif '#include <teakra/disassembler.h>' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/disassembler.h"\n')
            changed = True
        else:
            new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

files_to_check = [
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp',
    'src/core/melonds/teakra/src/test_verifier/main.cpp',
    'src/core/melonds/teakra/src/coff_reader/main.cpp',
]

for f in files_to_check:
    fix_file(f)
