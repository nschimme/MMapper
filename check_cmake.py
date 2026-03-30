import re
with open('src/CMakeLists.txt', 'r') as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if '${' in line and '}' not in line:
        print(f'{i+1}: {line.strip()}')
