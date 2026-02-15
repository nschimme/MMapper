// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

out vec2 vScreenPos;

void main()
{
    // Full-screen triangle using gl_VertexID
    // 0: (-1, -1), 1: (3, -1), 2: (-1, 3)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    vec2 screenPos = vec2(x, y);
    gl_Position = vec4(screenPos, 0.0, 1.0);
    vScreenPos = screenPos;
}
