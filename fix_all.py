import re
import os

# Fix OpenGLProber.cpp
with open('src/opengl/OpenGLProber.cpp', 'r') as f:
    lines = f.readlines()

new_lines = []
in_wasm_skip = False
for line in lines:
    if 'QProcess process;' in line:
        new_lines.append('#ifndef EMSCRIPTEN\n')
        new_lines.append(line)
        in_wasm_skip = True
    elif 'QByteArray output = process.readAllStandardOutput();' in line:
        new_lines.append(line)
        new_lines.append('#endif\n')
        in_wasm_skip = False
    elif 'QJsonDocument doc = QJsonDocument::fromJson(output);' in line:
        new_lines.append('#ifndef EMSCRIPTEN\n')
        new_lines.append(line)
    elif 'MMLOG_INFO() << "[GL Probe] Survey determined:" << result.highestVersionString.c_str();' in line:
        new_lines.append(line)
        new_lines.append('#endif\n')
    else:
        new_lines.append(line)

# Wait, this is getting complicated. Let's just rewrite OpenGLProber.cpp correctly.
