// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP;

in vec4 aColor;
in vec3 aVert;

out vec4 vs_color_out; // Renamed for clarity when passing to Geometry Shader

void main()
{
    vs_color_out = aColor;
    gl_Position = uMVP * vec4(aVert, 1.0);
}
