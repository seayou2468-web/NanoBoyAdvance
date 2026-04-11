import os

files_to_fix = [
    'src/core/melonds/teakra/src/mod_test_generator/main.cpp',
    'src/core/melonds/teakra/src/dsp1_reader/main.cpp',
    'src/core/melonds/teakra/src/test_generator/main.cpp',
    'src/core/melonds/teakra/src/step2_test_generator/main.cpp',
    'src/core/melonds/teakra/src/test_verifier/main.cpp',
    'src/core/melonds/teakra/src/coff_reader/main.cpp',
    'src/core/melonds/teakra/src/makedsp1/main.cpp'
]

for f_path in files_to_fix:
    if not os.path.exists(f_path): continue
    with open(f_path, 'rb') as f:
        content = f.read()

    if b'int main' in content:
        # Check if already wrapped (might have been from previous run)
        if b'#if 0' in content:
            continue

        # Wrap the whole file with #if 0 since these are purely standalone utilities
        new_content = b'#if 0\n' + content + b'\n#endif\n'
        with open(f_path, 'wb') as f:
            f.write(new_content)
        print(f"Disabled all code in {f_path}")
