import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False

    dir_path = os.path.dirname(file_path)
    # Target is src/core/melonds/teakra/include
    # We are in src/core/melonds/teakra/src/...

    # Calculate depth from src/core/melonds/teakra/src
    parts = file_path.split('/')
    try:
        src_idx = parts.index('src', parts.index('teakra'))
        depth = len(parts) - src_idx - 1
        rel_prefix = '../' * depth + '../include/'
    except (ValueError, IndexError):
        if 'src/core/melonds' in dir_path:
            rel_prefix = 'teakra/include/'
        else:
            return

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
        print(f"Fixed {file_path} with {rel_prefix}")

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
