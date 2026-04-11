import os

plist_path = 'Config/Project.plist'
with open(plist_path, 'r') as f:
    content = f.read()

old_flags = '<key>LDECompilerFlags</key>\n\t<array>\n\t\t<string>-fobjc-arc</string>\n\t</array>'
new_flags = '<key>LDECompilerFlags</key>\n\t<array>\n\t\t<string>-fobjc-arc</string>\n\t\t<string>-Isrc/core/melonds</string>\n\t\t<string>-Isrc/core/melonds/teakra/include</string>\n\t</array>'

if old_flags in content:
    content = content.replace(old_flags, new_flags)
    with open(plist_path, 'w') as f:
        f.write(content)
    print("Fixed Config/Project.plist")
else:
    print("Could not find LDECompilerFlags in expected format")
