// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform float uTime;
uniform vec4 uWeatherIntensities; // x: rain, y: snow, z: clouds, w: fog
uniform vec4 uTimeOfDayColor;
uniform vec3 uPlayerPos;
uniform sampler2D uNoiseTexture;

in vec2 vWorldPos;
out vec4 vFragmentColor;

void main()
{
    vec2 worldPos = vWorldPos;

    // Soft distance mask to fade out weather far from the player
    float distToPlayer = distance(worldPos, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 10.0, distToPlayer);

    // Boost visibility during dark hours (dusk/night)
    float darkBoost = 1.0 + uTimeOfDayColor.a * 1.5;

    vec4 weatherColor = vec4(0.0);

    // Sample pre-calculated tiling noise in world space
    // R: Fog noise, G: Cloud noise

    // Fog scrolling - 3 octaves for dynamic billowing
    vec2 fogUV1 = worldPos * 0.08 + uTime * 0.12;
    vec2 fogUV2 = worldPos * 0.04 - uTime * 0.05;
    vec2 fogUV3 = worldPos * 0.12 + uTime * 0.08;
    float fogN1 = texture(uNoiseTexture, fogUV1 * 0.06).r;
    float fogN2 = texture(uNoiseTexture, fogUV2 * 0.1).r;
    float fogN3 = texture(uNoiseTexture, fogUV3 * 0.04).r;
    float fogN = (fogN1 * 0.5) + (fogN2 * 0.3) + (fogN3 * 0.2);

    // Cloud scrolling
    vec2 cloudUV = worldPos * 0.18 - uTime * 0.06;
    float cloudN = texture(uNoiseTexture, cloudUV * 0.08).g;

    // Fog: soft billowing noise
    float fogInt = uWeatherIntensities.w;
    if (fogInt > 0.0) {
        weatherColor = vec4(0.8, 0.8, 0.85, fogInt * fogN * 0.9 * darkBoost * localMask);
    }

    // Clouds: puffy high-contrast noise
    float cloudsInt = uWeatherIntensities.z;
    if (cloudsInt > 0.0) {
        // Puffier and sparser: higher threshold and sharper transition
        float puffy = smoothstep(0.60, 0.70, cloudN);
        vec4 clouds = vec4(0.9, 0.9, 1.0, cloudsInt * puffy * 0.25 * darkBoost * localMask);
        weatherColor.rgb = mix(weatherColor.rgb, clouds.rgb, clouds.a);
        weatherColor.a = max(weatherColor.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    vec4 result = vec4(todTint, todAlpha);

    // Blend weather on top of ToD
    if (weatherColor.a > 0.0) {
        result.rgb = mix(result.rgb, weatherColor.rgb, weatherColor.a);
        result.a = max(result.a, weatherColor.a);
    }

    vFragmentColor = result;
}
