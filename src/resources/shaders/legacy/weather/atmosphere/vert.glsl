// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    mat4 uInvViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uWeatherIntensities; // x=rain, y=snow, z=clouds, w=fog
    vec4 uTimeOfDayColor;
    vec4 uTimeAndDelta; // x=time, y=deltaTime
};

out vec3 vWorldPos;

void main()
{
    // Full-screen triangle using gl_VertexID
    // 0: (-1, -1), 1: (3, -1), 2: (-1, 3)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    gl_Position = vec4(x, y, 0.0, 1.0);

    // Unproject to world space at the player's Z layer
    vec4 near4 = uInvViewProj * vec4(x, y, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(x, y, 1.0, 1.0);
    vec3 nearPos = near4.xyz / near4.w;
    vec3 farPos = far4.xyz / far4.w;

    float uZScale = uPlayerPos.w;
    float t = (uPlayerPos.z * uZScale - nearPos.z) / (farPos.z - nearPos.z);
    vWorldPos = mix(nearPos, farPos, t);
    vWorldPos.z /= uZScale;
}
