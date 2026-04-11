import os

files_to_fix = [
    'src/core/melonds/teakra/src/teakra.cpp',
    'src/core/melonds/teakra/src/teakra_c.cpp',
    'src/core/melonds/teakra/src/disassembler.cpp',
    'src/core/melonds/teakra/src/disassembler_c.cpp'
]

for file_path in files_to_fix:
    if not os.path.exists(file_path):
        print(f"Skipping {file_path}, not found.")
        continue

    with open(file_path, 'r') as f:
        lines = f.readlines()

    new_lines = []
    changed = False
    for line in lines:
        if '#include "teakra/teakra.h"' in line:
            new_lines.append('#include "../include/teakra/teakra.h"\n')
            changed = True
        elif '#include "teakra/teakra_c.h"' in line:
            new_lines.append('#include "../include/teakra/teakra_c.h"\n')
            changed = True
        elif '#include "teakra/disassembler.h"' in line:
            new_lines.append('#include "../include/teakra/disassembler.h"\n')
            changed = True
        elif '#include "teakra/disassembler_c.h"' in line:
            new_lines.append('#include "../include/teakra/disassembler_c.h"\n')
            changed = True
        else:
            new_lines.append(line)

    if changed:
        with open(file_path, 'w') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")
