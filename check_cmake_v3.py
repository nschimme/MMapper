import re
import os

def check_file(filepath):
    if not os.path.exists(filepath):
        return
    with open(filepath, 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        # Very simple check: if line contains ${ but not }, it might be split.
        # But also count them to be sure.
        if line.count('${') > line.count('}'):
             print(f"Split variable at {filepath}:{i+1}: {line.strip()}")

check_file('src/CMakeLists.txt')
check_file('CMakeLists.txt')
for root, dirs, files in os.walk('cmake'):
    for file in files:
        if file.endswith('.cmake'):
            check_file(os.path.join(root, file))
