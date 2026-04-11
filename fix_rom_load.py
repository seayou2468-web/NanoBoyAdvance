import sys

file_path = 'src/core/melonds/runtime.cpp'
with open(file_path, 'r') as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    if 'NDS::LoadBIOS();' in line:
        continue # Remove the first LoadBIOS
    if 'runtime.rom_loaded = NDS::LoadCart(' in line:
        new_lines.append(line)
        # Add LoadBIOS after LoadCart
        new_lines.append('  NDS::LoadBIOS();\n')
    else:
        new_lines.append(line)

with open(file_path, 'w') as f:
    f.writelines(new_lines)
