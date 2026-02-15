// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform float uTime;
uniform vec4 uWeatherIntensities; // x: rain, y: snow, z: clouds, w: fog
uniform vec4 uTimeOfDayColor;
uniform vec3 uPlayerPos;
uniform float uZScale;
uniform sampler2D uNoiseTexture;

in vec3 vNearPos;
in vec3 vFarPos;
out vec4 vFragmentColor;

vec3 intersect(float targetZ)
{
    float diff = vFarPos.z - vNearPos.z;
    if (abs(diff) < 0.0001) return vNearPos;
    float t = (targetZ - vNearPos.z) / diff;
    return mix(vNearPos, vFarPos, t);
}

void main()
{
    float darkBoost = 1.0 + uTimeOfDayColor.a * 1.5;
    vec4 finalWeather = vec4(0.0);

    // 1. Fog: at ground level (player Z)
    float groundZ = uPlayerPos.z * uZScale;
    vec3 groundWorld = intersect(groundZ);
    float distToPlayerGround = distance(groundWorld.xy, uPlayerPos.xy);
    float fogMask = smoothstep(12.0, 10.0, distToPlayerGround);

    if (uWeatherIntensities.w > 0.0) {
        // Multi-octave billowing fog
        vec2 uv1 = groundWorld.xy * 0.15 + uTime * 0.12;
        vec2 uv2 = groundWorld.xy * 0.07 - uTime * 0.05;
        float n1 = texture(uNoiseTexture, uv1 * 0.05).r;
        float n2 = texture(uNoiseTexture, uv2 * 0.1).r;
        float fogN = (n1 * 0.6) + (n2 * 0.4);

        vec4 fogColor = vec4(0.8, 0.8, 0.85, uWeatherIntensities.w * fogN * 0.8 * darkBoost * fogMask);
        finalWeather = fogColor;
    }

    // 2. Clouds: at higher elevation
    // We only render them if the eye (vNearPos) is below the clouds
    // Actually, usually camera is above. If camera is above clouds, we zoom "past" them.
    // Let's place clouds at playerZ + 40.0
    float skyZ = (uPlayerPos.z + 40.0) * uZScale;

    // Only draw if we haven't zoomed past them (vNearPos.z is eye position in some sense)
    // In perspective, vNearPos is on the near clip plane.
    // If SkyZ is between Near and Far, we see it.
    if (uWeatherIntensities.z > 0.0 && vNearPos.z > skyZ) {
        vec3 skyWorld = intersect(skyZ);
        float distToPlayerSky = distance(skyWorld.xy, uPlayerPos.xy);
        float skyMask = smoothstep(15.0, 12.0, distToPlayerSky);

        // Smaller, sparser clouds (higher frequency)
        vec2 uvC = skyWorld.xy * 0.3 - uTime * 0.04;
        float cloudN = texture(uNoiseTexture, uvC * 0.12).g;
        float puffy = smoothstep(0.62, 0.72, cloudN);

        vec4 clouds = vec4(0.9, 0.9, 1.0, uWeatherIntensities.z * puffy * 0.3 * darkBoost * skyMask);

        // Blend clouds over fog
        finalWeather.rgb = mix(finalWeather.rgb, clouds.rgb, clouds.a);
        finalWeather.a = max(finalWeather.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;
    vec4 result = vec4(todTint, todAlpha);

    // Blend weather on top
    if (finalWeather.a > 0.0) {
        result.rgb = mix(result.rgb, finalWeather.rgb, finalWeather.a);
        result.a = max(result.a, finalWeather.a);
    }

    vFragmentColor = result;
}
