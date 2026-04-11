import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False

    for line in lines:
        if '#include "' in line and 'teakra/' in line:
            # Revert to standard include path
            start_quote = line.find('"')
            end_quote = line.find('"', start_quote + 1)
            path = line[start_quote+1:end_quote]
            if 'teakra/' in path:
                new_path = 'teakra/' + path.split('teakra/')[-1]
                if new_path != path:
                    new_lines.append(f'#include "{new_path}"\n')
                    changed = True
                    continue
        new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Reverted {file_path}")

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
