import os

TEAKRA_INCLUDE_BASE = 'src/core/melonds/teakra/include'

def get_relative_path(from_file, to_header_path):
    header_abs_path = os.path.abspath(os.path.join(TEAKRA_INCLUDE_BASE, to_header_path))
    file_abs_dir = os.path.abspath(os.path.dirname(from_file))
    rel_path = os.path.relpath(header_abs_path, file_abs_dir)
    return rel_path

def process_file(file_path):
    if not os.path.isfile(file_path):
        return

    try:
        with open(file_path, 'rb') as f:
            data = f.read()
    except Exception:
        return

    teakra_headers = [
        b'teakra/teakra.h',
        b'teakra/teakra_c.h',
        b'teakra/disassembler.h',
        b'teakra/disassembler_c.h'
    ]

    changed = False
    lines = data.splitlines(keepends=True)
    new_lines = []

    for line in lines:
        new_line = line
        for header in teakra_headers:
            # We look for #include "header" or #include <header>
            quoted = b'"' + header + b'"'
            angled = b'<' + header + b'>'

            if quoted in line or angled in line:
                rel = get_relative_path(file_path, header.decode()).encode()
                new_line = line.replace(quoted, b'"' + rel + b'"').replace(angled, b'"' + rel + b'"')
                changed = True
                break
        new_lines.append(new_line)

    if changed:
        with open(file_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed {file_path}")

for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c')):
            process_file(os.path.join(root, file))
