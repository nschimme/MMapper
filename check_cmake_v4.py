import os

def check_file(filepath):
    if not os.path.exists(filepath):
        return
    with open(filepath, 'r') as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        if line.count('${') != line.count('}'):
             print(f"Mismatch at {filepath}:{i+1}: {line.strip()}")

check_file('src/CMakeLists.txt')
check_file('CMakeLists.txt')
