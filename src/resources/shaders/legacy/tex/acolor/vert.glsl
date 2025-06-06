// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP;

in vec4 aColor; // Changed from attribute
in vec2 aTexCoord; // Changed from attribute
in vec3 aVert; // Changed from attribute

out vec4 vColor; // Changed from varying
out vec2 vTexCoord; // Changed from varying

void main()
{
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = uMVP * vec4(aVert, 1.0);
}
