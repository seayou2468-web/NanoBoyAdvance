import os
import re

# Base path where headers actually reside
REPO_ROOT = os.path.abspath('.')
HDR_BASE = os.path.join(REPO_ROOT, 'src/core/melonds/teakra/include/teakra')
TARGETS = ['teakra.h', 'teakra_c.h', 'disassembler.h', 'disassembler_c.h']

def get_rel(from_file, h_name):
    h_abs = os.path.join(HDR_BASE, h_name)
    f_dir = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(h_abs, f_dir).replace('\\', '/')

for root, dirs, files in os.walk('src/core/melonds'):
    for file in files:
        if not file.endswith(('.cpp', '.h', '.hpp', '.c')): continue
        f_path = os.path.join(root, file)
        with open(f_path, 'rb') as f: data = f.read()
        lines = data.splitlines(keepends=True)
        new_lines = []
        changed = False
        for line in lines:
            m = re.search(rb'#include\s*[<"]([^>"]+)[>"]', line)
            if m:
                path_str = m.group(1).decode('utf-8', 'ignore')
                bname = os.path.basename(path_str)
                if bname in TARGETS:
                    correct = get_rel(f_path, bname)
                    if path_str != correct:
                        # Construct exact include line
                        indent_m = re.search(rb'^(\s*)#include', line)
                        indent = indent_m.group(1) if indent_m else b''
                        line = indent + b'#include "' + correct.encode() + b'"\n'
                        changed = True
            new_lines.append(line)
        if changed:
            with open(f_path, 'wb') as f: f.writelines(new_lines)
            print(f"Fixed {f_path}")
