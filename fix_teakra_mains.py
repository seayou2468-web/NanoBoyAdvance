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
        data = f.read()

    if b'int main' in data and b'#ifdef NO_MAIN' not in data:
        # Wrap main with #ifndef NO_MAIN
        # Simple string replacement for start of main
        data = data.replace(b'int main', b'#ifndef NO_MAIN\nint main', 1)
        # Find the end of file to add #endif, or just append it.
        # Usually these are small utility files.
        data += b'\n#endif // NO_MAIN\n'

        with open(f_path, 'wb') as f:
            f.write(data)
        print(f"Wrapped main in {f_path}")
