// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

uniform mat4 uMVP;

layout(location = 0) in vec2 aTexCoord;
layout(location = 1) in vec3 aVert;
layout(location = 2) in mat4 aInstanceMatrix;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = uMVP * aInstanceMatrix * vec4(aVert, 1.0);
}
