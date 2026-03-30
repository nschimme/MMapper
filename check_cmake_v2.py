import re

def check_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Check for split ${ ... }
    lines = content.split('\n')
    for i, line in enumerate(lines):
        if line.count('${') > line.count('}'):
             print(f"Split variable at {filepath}:{i+1}: {line.strip()}")
        if line.count('(') > line.count(')'):
             # This is common in CMake for long commands, but let's check for weird ones
             pass

    # Check for literal "${" followed by newline
    if re.search(r'\$\{\s*$', content, re.MULTILINE):
        print(f"Literal split variable at {filepath}")

check_file('src/CMakeLists.txt')
check_file('CMakeLists.txt')
