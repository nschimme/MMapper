// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uInvViewProj;
uniform vec3 uPlayerPos;
uniform float uZScale;

out vec3 vNearPos;
out vec3 vFarPos;

void main()
{
    // Full-screen triangle using gl_VertexID
    // 0: (-1, -1), 1: (3, -1), 2: (-1, 3)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    vec2 screenPos = vec2(x, y);
    gl_Position = vec4(screenPos, 0.0, 1.0);

    // Pass projected positions to fragment shader for multi-plane intersection
    vec4 near4 = uInvViewProj * vec4(screenPos, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(screenPos, 1.0, 1.0);
    vNearPos = near4.xyz / near4.w;
    vFarPos = far4.xyz / far4.w;
}
