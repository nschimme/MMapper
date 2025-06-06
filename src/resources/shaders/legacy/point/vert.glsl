// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP;
uniform float uPointSize;

in vec4 aColor; // Changed from attribute
in vec3 aVert;  // Changed from attribute

out vec4 vColor; // Changed from varying

void main()
{
    vColor = aColor;
    gl_PointSize = uPointSize;
    gl_Position = uMVP * vec4(aVert, 1.0);
}
