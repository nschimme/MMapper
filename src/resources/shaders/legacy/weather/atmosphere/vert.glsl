// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos;   // xyz, w=zScale
    vec4 uIntensities; // precip, clouds, fog, type
    ivec4 uToDIndices; // x=start, y=target
    vec4 uTimes;       // x=time, y=delta, z=todLerp
};

uniform mat4 uInvViewProj;

out vec2 vNDC;
out vec3 vWorldPos;

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

    // Unproject to Z=0 world plane for geostabilization
    vec4 near4 = uInvViewProj * vec4(pos, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(pos, 1.0, 1.0);
    vec3 nearPos = near4.xyz / near4.w;
    vec3 farPos = far4.xyz / far4.w;

    float t = (0.0 - nearPos.z) / (farPos.z - nearPos.z);
    vWorldPos = mix(nearPos, farPos, t);

    gl_Position = vec4(pos, 0.0, 1.0);
}
