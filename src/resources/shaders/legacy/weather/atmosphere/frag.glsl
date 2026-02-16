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

uniform sampler2D uNoiseTex;

in vec2 vNDC;
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
    // Unproject NDC to world space on the player's Z plane
    vec4 near4 = uInvViewProj * vec4(vNDC, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(vNDC, 1.0, 1.0);
    vec3 nearPos = near4.xyz / near4.w;
    vec3 farPos = far4.xyz / far4.w;

    float uZScale = uPlayerPos.w;
    float t = (uPlayerPos.z * uZScale - nearPos.z) / (farPos.z - nearPos.z);
    vec3 worldPos = mix(nearPos, farPos, t);
    worldPos.z /= uZScale;

    float distToPlayer = distance(worldPos.xy, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 8.0, distToPlayer);

    vec4 weatherColor = vec4(0.0);

    float uTime = uTimeAndDelta.x;
    float uCloudsIntensity = get_intensity(2);
    float uFogIntensity = get_intensity(3);
    vec4 uTimeOfDayColor = get_tod_color();

    // Fog: soft drifting noise
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.15 + uTime * 0.1);
        // Density increases non-linearly with intensity
        float density = 0.4 + uFogIntensity * 0.4;
        weatherColor = vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * density);
        // Emissive boost at night
        weatherColor.rgb += uTimeOfDayColor.a * 0.15;
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
        weatherColor.rgb = mix(weatherColor.rgb, clouds.rgb, clouds.a);
        weatherColor.a = max(weatherColor.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    vec4 result = vec4(todTint, todAlpha);

    // Blend weather on top of ToD without tinting it
    if (weatherColor.a > 0.0) {
        float Ta = todAlpha;
        float Wa = weatherColor.a;
        // combinedAlpha = Ta + Wa - Ta * Wa;
        float combinedAlpha = 1.0 - (1.0 - Ta) * (1.0 - Wa);
        result.a = combinedAlpha;
        // result.rgb = (T * Ta * (1 - Wa) + W * Wa) / result.a
        result.rgb = (todTint * Ta * (1.0 - Wa) + weatherColor.rgb * Wa)
                     / max(combinedAlpha, 0.001);
    }

    vFragmentColor = result;
}
