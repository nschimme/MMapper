// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aQuadPos; // (-0.5, -0.5) to (0.5, 0.5)
layout(location = 1) in vec2 aParticlePos;
layout(location = 2) in float aHash;
layout(location = 3) in float aType;
layout(location = 4) in float aLife;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    mat4 uInvViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uWeatherIntensities; // x=rain, y=snow, z=clouds, w=fog
    vec4 uTimeOfDayColor;
    vec4 uViewport; // xy=offset, zw=size
    vec4 uTimeAndDelta; // x=time, y=deltaTime
};

out float vType;
out float vLife;
out vec2 vLocalCoord;
out float vLocalMask;

void main()
{
    float uTime = uTimeAndDelta.x;
    float uZScale = uPlayerPos.w;

    vType = aType;
    vLife = aLife;
    vLocalCoord = aQuadPos + 0.5;

    vec2 size;
    vec2 pos = aParticlePos;

    if (aType == 0.0) { // Rain
        size = vec2(1.0 / 12.0, 1.0 / 0.15);
    } else { // Snow
        size = vec2(1.0 / 4.0, 1.0 / 4.0);
        // Apply sinusoidal swaying to match simulation better
        pos.x += sin(uTime * 1.2 + aHash * 6.28) * 0.4;
    }

    vec3 worldPos = vec3(pos + aQuadPos * size, uPlayerPos.z);

    float distToPlayer = distance(pos, uPlayerPos.xy);
    vLocalMask = smoothstep(12.0, 8.0, distToPlayer);

    gl_Position = uViewProj * vec4(worldPos.x, worldPos.y, worldPos.z * uZScale, 1.0);
}
