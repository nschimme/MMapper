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
    vec4 uPlayerPos;        // xyz, w=zScale
    vec4 uIntensities;      // precip_start, clouds_start, fog_start, type_start
    vec4 uTargets;          // precip_target, clouds_target, fog_target, type_target
    vec4 uTimeOfDayIndices; // x=startIdx, y=targetIdx, z=timeOfDayIntensityStart, w=timeOfDayIntensityTarget
    vec4 uConfig;           // x=weatherStartTime, y=timeOfDayStartTime, z=duration, w=unused
    vec4 uTorch;            // x=startIntensity, y=targetIntensity, z=colorIdx, w=unused
};

layout(std140) uniform TimeBlock
{
    vec2 uTime; // x=time, y=delta
};

in vec3 vWorldPos;
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

    vec4 result = uTimeOfDayColor;

    float artificialLightIntensity = mix(uTorch.x, uTorch.y, timeOfDayLerp);
    // Trigger flicker if artificial light is present AND (it's globally dark OR current room is dark)
    if (artificialLightIntensity > 0.0 && (currentTimeOfDayIntensity > 0.01 || uTorch.w > 0.5)) {
        float dist = distance(vWorldPos.xy, uPlayerPos.xy);
        // Torch radius: leak into next few rooms (approx 3.5 units)
        float glow = smoothstep(3.5, 0.0, dist) * artificialLightIntensity;

        // Flicker: torch-like irregular oscillation
        float flicker = 1.0 + 0.07 * sin(uCurrentTime * 15.0) + 0.05 * sin(uCurrentTime * 31.0);
        glow *= flicker;

        vec4 torchColor = uNamedColors[int(uTorch.z)];

        // Blend torch glow: mix color and reduce darkness alpha
        result = mix(result, torchColor, glow * torchColor.a);
        result.a *= (1.0 - glow * 0.8);
    }

    vFragmentColor = result;
}
