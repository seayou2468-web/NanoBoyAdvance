import os

file_path = 'src/core/melonds/DSi_DSP.cpp'
with open(file_path, 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if '#include "teakra/include/teakra/include/teakra/teakra.h"' in line:
        new_lines.append('#include "teakra/include/teakra/teakra.h"\n')
    else:
        new_lines.append(line)

with open(file_path, 'w') as f:
    f.writelines(new_lines)
