// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uInvViewProj;
uniform vec3 uPlayerPos;
uniform float uZScale;

out vec2 vWorldPos;
out vec2 vTexCoord;

void main()
{
    // Full-screen triangle using gl_VertexID
    // 0: (-1, -1), 1: (3, -1), 2: (-1, 3)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    vec2 screenPos = vec2(x, y);
    gl_Position = vec4(screenPos, 0.0, 1.0);
    vTexCoord = screenPos * 0.5 + 0.5;

    // Reconstruct world position on the player's plane
    vec4 near4 = uInvViewProj * vec4(screenPos, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(screenPos, 1.0, 1.0);
    vec3 nearPos = near4.xyz / near4.w;
    vec3 farPos = far4.xyz / far4.w;

    float t = (uPlayerPos.z * uZScale - nearPos.z) / (farPos.z - nearPos.z);
    vWorldPos = mix(nearPos, farPos, t).xy;
}
