import sys

def fix_file(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()

    new_lines = []
    for line in lines:
        if 'void Configuration::CanvasSettings::Advanced::registerChangeCallback' in line and 'const' not in line:
            new_lines.append(line.replace(')', ') const'))
        elif 'void Configuration::CanvasSettings::registerChangeCallback' in line and 'const' not in line:
            new_lines.append(line.replace(')', ') const'))
        elif 'void Configuration::NamedColorOptions::registerChangeCallback' in line and 'const' not in line:
            new_lines.append(line.replace(')', ') const'))
        elif 'void Configuration::registerChangeCallback' in line and 'const' not in line:
            new_lines.append(line.replace(')', ') const'))
        else:
            new_lines.append(line)

    with open(filename, 'w') as f:
        f.writelines(new_lines)

fix_file('src/configuration/configuration.cpp')
