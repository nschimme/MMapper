// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

uniform mat4 uInvViewProj;
uniform vec3 uPlayerPos;
uniform float uZScale;
uniform ivec4 uPhysViewport;
uniform float uTime;
uniform float uCloudsIntensity;
uniform float uFogIntensity;
uniform vec4 uTimeOfDayColor;
uniform sampler2D uNoiseTex;

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
    // Reconstruct world position on the player's plane
    vec2 screenPos = (gl_FragCoord.xy / vec2(uPhysViewport.zw)) * 2.0 - 1.0;
    vec4 near4 = uInvViewProj * vec4(screenPos, -1.0, 1.0);
    vec4 far4 = uInvViewProj * vec4(screenPos, 1.0, 1.0);
    vec3 nearPos = near4.xyz / near4.w;
    vec3 farPos = far4.xyz / far4.w;

    float t = (uPlayerPos.z * uZScale - nearPos.z) / (farPos.z - nearPos.z);
    vec3 worldPos = mix(nearPos, farPos, t);
    worldPos.z /= uZScale;

    float distToPlayer = distance(worldPos.xy, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 8.0, distToPlayer);

    vec4 weatherColor = vec4(0.0);

    // Fog: soft drifting noise
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.15 + uTime * 0.1);
        weatherColor = vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * 0.6);
        // Emissive boost at night
        weatherColor.rgb += uTimeOfDayColor.a * 0.15;
    }

    // Clouds: puffy high-contrast noise
    if (uCloudsIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.06 - uTime * 0.03);
        float puffy = smoothstep(0.45, 0.55, n);
        vec4 clouds = vec4(0.9, 0.9, 1.0, uCloudsIntensity * puffy * localMask * 0.4);
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
