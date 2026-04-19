from pathlib import Path

root = Path('src')
count = 0
for path in root.rglob('*.*'):
    if path.suffix.lower() not in {'.h', '.hpp', '.cpp', '.c', '.cc', '.cxx'}:
        continue
    text = path.read_text(encoding='utf-8', errors='replace')
    if '#include "span"' in text:
        new_text = text.replace('#include "span"', '#include <span>')
        if new_text != text:
            path.write_text(new_text, encoding='utf-8')
            count += 1
            print(f'patched {path}')
print(f'patched {count} files')
