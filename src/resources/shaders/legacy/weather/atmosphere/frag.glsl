// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform vec3 uPlayerPos;
uniform float uTime;
uniform vec4 uWeatherIntensities; // x: rain, y: snow, z: clouds, w: fog
uniform vec4 uTimeOfDayColor;
uniform sampler2D uNoiseTexture;

in vec2 vWorldPos;
out vec4 vFragmentColor;

void main()
{
    vec2 worldPos = vWorldPos;

    float distToPlayer = distance(worldPos, uPlayerPos.xy);
    float localMask = smoothstep(12.0, 8.0, distToPlayer);

    // Boost visibility during dark hours (dusk/night)
    float darkBoost = 1.0 + uTimeOfDayColor.a * 1.5;

    vec4 weatherColor = vec4(0.0);

    // Sample pre-calculated tiling noise
    // R: Fog noise, G: Cloud noise

    // Fog scrolling
    vec2 fogUV = worldPos * 0.15 + uTime * 0.1;
    float fogN = texture(uNoiseTexture, fogUV * 0.05).r;

    // Cloud scrolling
    vec2 cloudUV = worldPos * 0.06 - uTime * 0.03;
    float cloudN = texture(uNoiseTexture, cloudUV * 0.05).g;

    // Fog: soft drifting noise
    float fogInt = uWeatherIntensities.w;
    if (fogInt > 0.0) {
        weatherColor = vec4(0.8, 0.8, 0.85, fogInt * fogN * localMask * 0.6 * darkBoost);
    }

    // Clouds: puffy high-contrast noise
    float cloudsInt = uWeatherIntensities.z;
    if (cloudsInt > 0.0) {
        // Puffier and sparser: higher threshold and sharper transition
        float puffy = smoothstep(0.52, 0.62, cloudN);
        vec4 clouds = vec4(0.9, 0.9, 1.0, cloudsInt * puffy * localMask * 0.5 * darkBoost);
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
