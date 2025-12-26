// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019-2025 The MMapper Authors

uniform mat4 uMVP;

layout(location = 0) in vec4 aColor;
layout(location = 1) in ivec3 aVert;

out vec4 vColor;

void main()
{
    // ccw-order assumes it's a triangle fan (as opposed to a triangle strip)
    const ivec3[4] ioffsets_ccw = ivec3[4](ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 1, 0), ivec3(0, 1, 0));
    ivec3 ioffset = ioffsets_ccw[gl_VertexID];

    vColor = aColor;
    gl_Position = uMVP * vec4(aVert + ioffset, 1.0);
}
