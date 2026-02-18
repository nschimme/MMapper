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
    vec4 uIntensities; // precip_start, clouds_start, fog_start, type_start
    vec4 uTargets;     // precip_target, clouds_target, fog_target, type_target
    vec4 uTimeOfDayIndices; // x=startIdx, y=targetIdx, z=todIntStart, w=todIntTarget
    vec4 uConfig;      // x=weatherStartTime, y=timeOfDayStartTime, z=duration, w=unused
};

layout(std140) uniform TimeBlock
{
    vec2 uTime;   // x=time, y=delta
};

uniform sampler2D uNoiseTex;

in vec3 vWorldPos;
out vec4 vFragmentColor;

float get_noise(vec2 p)
{
    return texture(uNoiseTex, p / 256.0).r;
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    for (int i = 0; i < 4; ++i) {
        v += a * get_noise(p);
        p = p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

void main()
{
    vec3 worldPos = vWorldPos;

    float distToPlayer = distance(worldPos.xy, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 8.0, distToPlayer);

    float uCurrentTime = uTime.x;
    float uWeatherStartTime = uConfig.x;
    float uTimeOfDayStartTime = uConfig.y;
    float uTransitionDuration = uConfig.z;

    float weatherLerp = clamp((uCurrentTime - uWeatherStartTime) / uTransitionDuration, 0.0, 1.0);
    float uCloudsIntensity = mix(uIntensities[1], uTargets[1], weatherLerp);
    float uFogIntensity = mix(uIntensities[2], uTargets[2], weatherLerp);

    float timeOfDayLerp = clamp((uCurrentTime - uTimeOfDayStartTime) / uTransitionDuration, 0.0, 1.0);
    float currentTimeOfDayIntensity = mix(uTimeOfDayIndices.z, uTimeOfDayIndices.w, timeOfDayLerp);
    vec4 todStart = uNamedColors[int(uTimeOfDayIndices.x)];
    vec4 todTarget = uNamedColors[int(uTimeOfDayIndices.y)];
    vec4 uTimeOfDayColor = mix(todStart, todTarget, timeOfDayLerp);
    uTimeOfDayColor.a *= currentTimeOfDayIntensity;

    // Atmosphere overlay is now transparent by default (TimeOfDay is drawn separately)
    vec4 result = vec4(0.0);

    // Fog: soft drifting noise
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.15 + uCurrentTime * 0.1);
        // Density increases non-linearly with intensity
        float density = 0.4 + uFogIntensity * 0.4;
        vec4 fog = vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * density);
        // Emissive boost at night
        fog.rgb += uTimeOfDayColor.a * 0.15;

        // Blend fog over result
        float combinedAlpha = 1.0 - (1.0 - result.a) * (1.0 - fog.a);
        result.rgb = (result.rgb * result.a * (1.0 - fog.a) + fog.rgb * fog.a) / max(combinedAlpha, 0.001);
        result.a = combinedAlpha;
    }

    // Clouds: puffy high-contrast noise
    if (uCloudsIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.06 - uCurrentTime * 0.03);
        float puffy = smoothstep(0.45, 0.55, n);
        // Clouds get darker and more "stormy" as intensity increases
        float storminess = 1.0 - uCloudsIntensity * 0.4;
        vec4 clouds = vec4(0.9 * storminess, 0.9 * storminess, 1.0 * storminess, uCloudsIntensity * puffy * localMask * 0.5);
        // Emissive boost at night
        clouds.rgb += uTimeOfDayColor.a * 0.1;

        // Blend clouds over result
        float combinedAlpha = 1.0 - (1.0 - result.a) * (1.0 - clouds.a);
        result.rgb = (result.rgb * result.a * (1.0 - clouds.a) + clouds.rgb * clouds.a) / max(combinedAlpha, 0.001);
        result.a = combinedAlpha;
    }

    vFragmentColor = result;
}
