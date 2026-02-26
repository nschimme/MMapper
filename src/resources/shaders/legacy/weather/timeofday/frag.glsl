// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

layout(std140) uniform NamedColorsBlock
{
    vec4 uNamedColors[32];
};

layout(std140) uniform TimeBlock
{
    vec4 uTime; // x=time, y=delta, zw=unused
};

layout(std140) uniform WeatherBlock
{
    vec4 uIntensities;      // precip_start, clouds_start, fog_start, type_start
    vec4 uTargets;          // precip_target, clouds_target, fog_target, type_target
    vec4 uTimeOfDayIndices; // x=startIdx, y=targetIdx, z=timeOfDayIntensityStart, w=timeOfDayIntensityTarget
    vec4 uConfig;           // x=weatherStartTime, y=timeOfDayStartTime, z=duration, w=unused
};

out vec4 vFragmentColor;

void main()
{
    float uCurrentTime = uTime.x;
    float uTimeOfDayStartTime = uConfig.y;
    float uTransitionDuration = uConfig.z;

    vec4 timeOfDayStart = uNamedColors[int(uTimeOfDayIndices.x)];
    vec4 timeOfDayTarget = uNamedColors[int(uTimeOfDayIndices.y)];

    float timeOfDayLerp = clamp((uCurrentTime - uTimeOfDayStartTime) / uTransitionDuration,
                                0.0,
                                1.0);
    float currentTimeOfDayIntensity = mix(uTimeOfDayIndices.z, uTimeOfDayIndices.w, timeOfDayLerp);

    vec4 uTimeOfDayColor = mix(timeOfDayStart, timeOfDayTarget, timeOfDayLerp);
    uTimeOfDayColor.a *= currentTimeOfDayIntensity;

    vFragmentColor = uTimeOfDayColor;
}
