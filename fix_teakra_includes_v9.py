import os
import re

REPO_ROOT = os.path.abspath('.')
TEAKRA_HDR_ROOT = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGETS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, header_name):
    hdr_abs = os.path.join(TEAKRA_HDR_ROOT, header_name)
    file_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(hdr_abs, file_dir).replace('\\', '/')

def process(f_path):
    with open(f_path, 'rb') as f:
        lines = f.readlines()

    new_lines = []
    changed = False
    for line in lines:
        new_line = line
        m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            path_str = m.group(1).decode('utf-8', 'ignore')
            basename = os.path.basename(path_str)
            if basename in TARGETS:
                correct_rel = get_rel(f_path, basename)
                if path_str != correct_rel:
                    # Construct clean include
                    indent = line[:line.find(b'#include')]
                    new_line = indent + b'#include "' + correct_rel.encode() + b'"\n'
                    changed = True
        new_lines.append(new_line)

    if changed:
        with open(f_path, 'wb') as f:
            f.writelines(new_lines)
        print(f"Fixed includes in {f_path}")

# Run for all files
for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if file.endswith(('.cpp', '.h', '.hpp', '.c', '.mm', '.m')):
            process(os.path.join(root, file))
