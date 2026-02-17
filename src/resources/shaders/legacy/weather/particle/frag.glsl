// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

layout(std140) uniform NamedColorsBlock
{
    vec4 uNamedColors[32];
};

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos;   // xyz, w=zScale
    vec4 uIntensities; // precip, clouds, fog, type
    ivec4 uToDIndices; // x=start, y=target
    vec4 uTimes;       // x=time, y=delta, z=todLerp
};

in float vLife;
in vec2 vLocalCoord;
in float vLocalMask;

out vec4 vFragmentColor;

void main()
{
    float pIntensity = uIntensities.x;
    float pType = uIntensities.w;

    vec4 todStart = uNamedColors[uToDIndices.x];
    vec4 todTarget = uNamedColors[uToDIndices.y];
    vec4 uTimeOfDayColor = mix(todStart, todTarget, uTimes.z);
    uTimeOfDayColor.a *= uTimes.w;

    float lifeFade = smoothstep(0.0, 0.15, vLife) * smoothstep(1.0, 0.85, vLife);

    // Rain visuals
    float streak = 1.0 - smoothstep(0.0, 0.15, abs(vLocalCoord.x - 0.5));
    float rainAlpha = mix(0.4, 0.7, clamp(pIntensity, 0.0, 1.0));
    vec4 rainColor = vec4(0.6, 0.6, 1.0, pIntensity * streak * vLocalMask * rainAlpha * lifeFade);
    rainColor.rgb += uTimeOfDayColor.a * 0.2;

    // Snow visuals
    float dist = distance(vLocalCoord, vec2(0.5));
    float flake = 1.0 - smoothstep(0.1, 0.2, dist);
    float snowAlpha = mix(0.6, 0.9, clamp(pIntensity, 0.0, 1.0));
    vec4 snowColor = vec4(1.0, 1.0, 1.1, pIntensity * flake * vLocalMask * snowAlpha * lifeFade);
    snowColor.rgb += uTimeOfDayColor.a * 0.3;

    // Interpolate visuals based on pType
    vec4 pColor = mix(rainColor, snowColor, pType);

    if (pColor.a <= 0.0) {
        discard;
    }

    vFragmentColor = pColor;
}
