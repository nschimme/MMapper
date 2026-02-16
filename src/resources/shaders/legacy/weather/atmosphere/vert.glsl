// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

out vec2 vNDC;

void main()
{
    // Full-screen triangle using gl_VertexID:
    // 0: (-1, -1)
    // 1: ( 3, -1)
    // 2: (-1,  3)
    vec2 pos = vec2(
        float((gl_VertexID << 1) & 2) * 2.0 - 1.0,
        float((gl_VertexID & 2)) * 2.0 - 1.0
    );
    vNDC = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
