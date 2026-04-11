import os
import re

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    # Target headers we care about
    targets = [b'teakra.h', b'teakra_c.h', b'disassembler.h', b'disassembler_c.h']

    for line in lines:
        if b'#include' in line:
            # Match #include "..." or #include <...>
            m = re.search(b'#include\s*[<"]([^>"]*teakra/[^>"]*)[>"]', line)
            if m:
                old_path = m.group(1)
                # Extract the actual header file name
                for target in targets:
                    if target in old_path:
                        file_dir = os.path.dirname(file_path)
                        header_abs = os.path.abspath(os.path.join('src/core/melonds/teakra/include/teakra', target.decode()))
                        file_abs_dir = os.path.abspath(file_dir)
                        rel = os.path.relpath(header_abs, file_abs_dir).encode()

                        if old_path != rel:
                            line = line.replace(old_path, rel)
                            # Ensure it uses " instead of < if we changed it
                            line = line.replace(b'<' + rel + b'>', b'"' + rel + b'"')
                            changed = True
                        break
        new_lines.append(line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            fix_file(os.path.join(root, file))
