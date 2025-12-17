// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

uniform mat4 uMVP;

layout(location = 0) in vec2 aTexCoord;
layout(location = 1) in vec3 aVert;
layout(location = 2) in mat4 aInstanceMatrix;
layout(location = 6) in vec4 aInstanceColor;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    vTexCoord = aTexCoord;
    vColor = aInstanceColor;
    gl_Position = uMVP * aInstanceMatrix * vec4(aVert, 1.0);
}
