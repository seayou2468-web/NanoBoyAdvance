import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False

    # Calculate relative path to teakra/include
    # This is a bit complex, let's just use what's appropriate for each file location

    dir_path = os.path.dirname(file_path)
    if 'teakra/src' in dir_path:
        rel_prefix = '../include/'
    elif dir_path == 'src/core/melonds':
        rel_prefix = 'teakra/include/'
    else:
        # For subdirectories like dsp1_reader/main.cpp
        rel_prefix = '../../include/'

    for line in lines:
        if '#include "teakra/teakra.h"' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/teakra.h"\n')
            changed = True
        elif '#include "teakra/teakra_c.h"' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/teakra_c.h"\n')
            changed = True
        elif '#include "teakra/disassembler.h"' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/disassembler.h"\n')
            changed = True
        elif '#include "teakra/disassembler_c.h"' in line:
            new_lines.append(f'#include "{rel_prefix}teakra/disassembler_c.h"\n')
            changed = True
        else:
            new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

files_to_check = [
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

for f in files_to_check:
    fix_file(f)
