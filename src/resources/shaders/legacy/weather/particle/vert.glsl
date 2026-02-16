// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aQuadPos; // (-0.5, -0.5) to (0.5, 0.5)
layout(location = 1) in vec2 aParticlePos;
layout(location = 2) in float aLife;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    mat4 uInvViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uIntensitiesStart;
    vec4 uIntensitiesTarget;
    vec4 uToDColorStart;
    vec4 uToDColorTarget;
    vec4 uTransitionStart; // x=weather, y=tod
    vec4 uTimeAndDelta;    // x=time, y=deltaTime
};

uniform float uType;
uniform int uInstanceOffset;

out float vType;
out float vLife;
out vec2 vLocalCoord;
out float vLocalMask;

float rand(float n)
{
    return fract(sin(n) * 43758.5453123);
}

float get_intensity(int idx)
{
    float uTime = uTimeAndDelta.x;
    float t = clamp((uTime - uTransitionStart.x) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uIntensitiesStart[idx], uIntensitiesTarget[idx], s);
}

void main()
{
    float uTime = uTimeAndDelta.x;
    float uZScale = uPlayerPos.w;

    float hash = rand(float(gl_InstanceID + uInstanceOffset));

    vType = uType;
    vLife = aLife;
    vLocalCoord = aQuadPos + 0.5;

    vec2 size;
    vec2 pos = aParticlePos;

    if (vType == 0.0) { // Rain
        float rainIntensity = get_intensity(0);
        // Streak length increases with intensity
        size = vec2(1.0 / 12.0, 1.0 / (0.25 - clamp(rainIntensity, 0.0, 2.0) * 0.1));
    } else { // Snow
        size = vec2(1.0 / 4.0, 1.0 / 4.0);
        // Apply sinusoidal swaying to match simulation better
        pos.x += sin(uTime * 1.2 + hash * 6.28) * 0.4;
    }

    vec3 worldPos = vec3(pos + aQuadPos * size, uPlayerPos.z);

    float distToPlayer = distance(pos, uPlayerPos.xy);
    vLocalMask = smoothstep(12.0, 8.0, distToPlayer);

    gl_Position = uViewProj * vec4(worldPos.x, worldPos.y, worldPos.z * uZScale, 1.0);
}
