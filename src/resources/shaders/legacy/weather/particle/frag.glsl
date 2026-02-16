// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

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

in float vType;
in float vLife;
in vec2 vLocalCoord;
in float vLocalMask;

out vec4 vFragmentColor;

float get_intensity(int idx)
{
    float uTime = uTimeAndDelta.x;
    float t = clamp((uTime - uTransitionStart.x) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uIntensitiesStart[idx], uIntensitiesTarget[idx], s);
}

vec4 get_tod_color()
{
    float uTime = uTimeAndDelta.x;
    float t = clamp((uTime - uTransitionStart.y) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uToDColorStart, uToDColorTarget, s);
}

void main()
{
    float uRainIntensity = get_intensity(0);
    float uSnowIntensity = get_intensity(1);
    vec4 uTimeOfDayColor = get_tod_color();

    vec4 pColor;
    float lifeFade = smoothstep(0.0, 0.15, vLife) * smoothstep(1.0, 0.85, vLife);

    if (vType == 0.0) { // Rain
        float streak = 1.0 - smoothstep(0.0, 0.15, abs(vLocalCoord.x - 0.5));
        float rainAlpha = mix(0.4, 0.7, clamp(uRainIntensity, 0.0, 1.0));
        pColor = vec4(0.6, 0.6, 1.0, uRainIntensity * streak * vLocalMask * rainAlpha * lifeFade);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.2;
    } else { // Snow
        float dist = distance(vLocalCoord, vec2(0.5));
        float flake = 1.0 - smoothstep(0.1, 0.2, dist);
        float snowAlpha = mix(0.6, 0.9, clamp(uSnowIntensity, 0.0, 1.0));
        pColor = vec4(1.0, 1.0, 1.1, uSnowIntensity * flake * vLocalMask * snowAlpha * lifeFade);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.3;

        // Spatial fading is now handled in simulation (vLife will be small in holes)
    }

    if (pColor.a <= 0.0) {
        discard;
    }

    vFragmentColor = pColor;
}
