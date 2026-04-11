import sys

file_path = 'src/core/melonds/runtime.cpp'
with open(file_path, 'r') as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if 'NDS::Init()' in line:
        new_lines.append(line)
    elif 'if (!NDS::Init()) {' in line:
        new_lines.append(line)
    elif 'Platform::DeInit();' in line and 'return nullptr;' in lines[lines.index(line)+1]:
        new_lines.append(line)
    elif 'GPU::RenderSettings render_settings{};' in line:
        new_lines.append('    GPU::InitRenderer(0);\n')
        new_lines.append(line)
    else:
        new_lines.append(line)

with open(file_path, 'w') as f:
    f.writelines(new_lines)
