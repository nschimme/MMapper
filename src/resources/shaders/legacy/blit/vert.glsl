// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

uniform mat4 uMVP;

layout(location = 0) in vec3 aVert;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aVert.xy * 0.5 + 0.5;
    gl_Position = uMVP * vec4(aVert, 1.0);
}
