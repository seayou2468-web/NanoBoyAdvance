import os
import re

def get_rel(from_file, to_header):
    # Base for headers is src/core/melonds/teakra/include/teakra/
    header_abs = os.path.abspath(os.path.join('src/core/melonds/teakra/include/teakra', to_header))
    file_dir_abs = os.path.abspath(os.path.dirname(from_file))
    return os.path.relpath(header_abs, file_dir_abs)

files_to_fix = {
    'src/core/melonds/DSi_DSP.cpp': ['teakra/teakra.h'],
    'src/core/melonds/teakra/src/disassembler_c.cpp': ['teakra/disassembler.h', 'teakra/disassembler_c.h'],
    'src/core/melonds/teakra/src/teakra_c.cpp': ['teakra/teakra.h', 'teakra/teakra_c.h'],
    'src/core/melonds/teakra/src/disassembler.cpp': ['teakra/disassembler.h'],
    'src/core/melonds/teakra/src/teakra.cpp': ['teakra/teakra.h'],
    'src/core/melonds/teakra/src/parser.cpp': ['teakra/disassembler.h'],
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp': ['teakra/disassembler.h'],
    'src/core/melonds/teakra/src/test_verifier/main.cpp': ['teakra/disassembler.h'],
    'src/core/melonds/teakra/src/coff_reader/main.cpp': ['teakra/disassembler.h']
}

for file_path, headers in files_to_fix.items():
    if not os.path.exists(file_path): continue
    with open(file_path, 'rb') as f:
        content = f.read()

    changed = False
    for h in headers:
        # Match any variation of include for this header
        # We look for something like #include "..." or #include <...> that ends with the header name
        pattern = rb'#include\s*[<"]([^>"]*' + h.encode() + rb')[>"]'

        def replace_func(match):
            nonlocal changed
            old_path = match.group(1)
            new_rel = get_rel(file_path, h.split('/')[-1]).encode()
            if old_path != new_rel:
                changed = True
                return b'#include "' + new_rel + b'"'
            return match.group(0)

        content = re.sub(pattern, replace_func, content)

    if changed:
        with open(file_path, 'wb') as f:
            f.write(content)
        print(f"Fixed {file_path}")
