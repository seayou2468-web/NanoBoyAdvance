import os

file_path = 'src/core/melonds/NDS.cpp'
with open(file_path, 'r') as f:
    content = f.read()

# Make sure MELONDS_VERSION is treated as a string even if it's not defined as one (though it should be)
# The issue seen earlier was a compilation error in NDS.cpp line 2839
old_line = 'static char const emuID[16] = "melonDS " MELONDS_VERSION;'
new_line = '''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;'''

if old_line in content:
    content = content.replace(old_line, new_line)
    with open(file_path, 'w') as f:
        f.write(content)
    print("Fixed NDS.cpp version macro usage")
