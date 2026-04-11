import os
path = 'src/core/melonds/NDS.cpp'
with open(path, 'rb') as f:
    data = f.read()

old = b'// NO$GBA debug register "Emulation ID"\n    if(addr >= 0x04FFFA00 && addr < 0x04FFFA10)\n    {\n        // FIX: GBATek says this should be padded with spaces\n        auto idx = addr - 0x04FFFA00;\n        return (u8)(emuID[idx]);\n    }'

new = b'''// NO$GBA debug register "Emulation ID"
    if(addr >= 0x04FFFA00 && addr < 0x04FFFA10)
    {
        // FIX: GBATek says this should be padded with spaces
#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;
        auto idx = addr - 0x04FFFA00;
        return (u8)(emuID[idx]);
    }'''

if old in data:
    data = data.replace(old, new)
else:
    # Try a slightly different version if match fails
    data = data.replace(b'return (u8)(emuID[idx]);', b'''#ifndef MELONDS_VERSION
#define MELONDS_VERSION "unknown"
#endif
        static char const emuID[16] = "melonDS " MELONDS_VERSION;
        return (u8)(emuID[idx]);''')

with open(path, 'wb') as f:
    f.write(data)
