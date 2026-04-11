import os

def fix_file(file_path):
    if not os.path.exists(file_path):
        return

    with open(file_path, 'rb') as f:
        data = f.read()

    lines = data.splitlines(keepends=True)
    new_lines = []
    changed = False

    for line in lines:
        # Search for any include that might be absolute or using wrong relative path
        if b'#include' in line and b'teakra/' in line:
            # Try to identify the teakra header name
            for header in [b'teakra.h', b'teakra_c.h', b'disassembler.h', b'disassembler_c.h']:
                if b'teakra/' + header in line:
                    # Calculate correct relative path
                    file_dir = os.path.dirname(file_path)
                    header_abs = os.path.abspath(os.path.join('src/core/melonds/teakra/include/teakra', header.decode()))
                    file_abs_dir = os.path.abspath(file_dir)
                    rel = os.path.relpath(header_abs, file_abs_dir).encode()

                    # Find what to replace. It could be "teakra/header", <teakra/header>, or some other variant.
                    # We'll just replace the whole path inside "" or <>
                    import re
                    m = re.search(b'["<](.*teakra/' + header + b')[">]', line)
                    if m:
                        old_path = m.group(1)
                        if old_path != rel:
                            line = line.replace(old_path, rel)
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
