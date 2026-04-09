import os

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Fix signatures in headers and source
    # This is a bit brute force but should work given the naming convention
    import re

    # Matches: void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback)
    # and variations
    pattern = r'(void\s+\w*(?:::\w*)*registerChangeCallback\s*\([^)]+\))(?!\s*const)'
    content = re.sub(pattern, r'\1 const', content)

    # Special case for ConfigValue.h if needed
    if 'ConfigValue.h' in filepath:
        content = content.replace('void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback)',
                                  'void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const')

    with open(filepath, 'w') as f:
        f.write(content)

files_to_fix = [
    'src/configuration/configuration.h',
    'src/configuration/configuration.cpp',
    'src/configuration/ConfigValue.h',
    'src/configuration/NamedConfig.h',
    'src/configuration/GroupConfig.h',
    'src/configuration/GroupConfig.cpp',
    'src/global/FixedPoint.h',
    'src/global/ChangeMonitor.h'
]

for f in files_to_fix:
    if os.path.exists(f):
        fix_file(f)
