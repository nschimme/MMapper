// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uIntensitiesStart;
    vec4 uIntensitiesTarget;
    vec4 uToDColorStart;
    vec4 uToDColorTarget;
    vec4 uTimes; // x=weatherStart, y=todStart, z=time, w=delta
};

uniform sampler2D uNoiseTex;

in vec2 vNDC;
in vec3 vWorldPos;
out vec4 vFragmentColor;

float get_intensity(int idx)
{
    float uTime = uTimes.z;
    float t = clamp((uTime - uTimes.x) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uIntensitiesStart[idx], uIntensitiesTarget[idx], s);
}

vec4 get_tod_color()
{
    float uTime = uTimes.z;
    float t = clamp((uTime - uTimes.y) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uToDColorStart, uToDColorTarget, s);
}

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

    float uTime = uTimes.z;
    float uCloudsIntensity = get_intensity(2);
    float uFogIntensity = get_intensity(3);
    vec4 uTimeOfDayColor = get_tod_color();

    // Start with Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;
    vec4 result = vec4(todTint, todAlpha);

    // Fog: soft drifting noise
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.15 + uTime * 0.1);
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
        float n = fbm(worldPos.xy * 0.06 - uTime * 0.03);
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
