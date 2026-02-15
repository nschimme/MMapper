// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uInvViewProj;
uniform vec3 uPlayerPos;
uniform float uZScale;
uniform ivec4 uPhysViewport;
uniform float uTime;
uniform float uCloudsIntensity;
uniform float uFogIntensity;
uniform vec4 uTimeOfDayColor;
uniform sampler2D uNoiseTexture;

out vec4 vFragmentColor;

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

    vec4 weatherAccum = vec4(0.0);

    // Fog: soft drifting noise (R channel)
    if (uFogIntensity > 0.01) {
        vec2 uv = worldPos.xy * 0.15 + uTime * 0.1;
        float n = texture(uNoiseTexture, uv * 0.05).r;
        vec4 fogColor = vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * 0.6);
        weatherAccum = fogColor;
    }

    // Clouds: puffy high-contrast noise (G channel)
    if (uCloudsIntensity > 0.01) {
        vec2 uv = worldPos.xy * 0.06 - uTime * 0.03;
        float n = texture(uNoiseTexture, uv * 0.05).g;
        // Puffier and sparser: higher threshold and sharper transition
        float puffy = smoothstep(0.52, 0.62, n);
        vec4 clouds = vec4(0.9, 0.9, 1.0, uCloudsIntensity * puffy * localMask * 0.5);
        weatherAccum.rgb = mix(weatherAccum.rgb, clouds.rgb, clouds.a);
        weatherAccum.a = max(weatherAccum.a, clouds.a);
    }

    // Time of Day tint
    vec3 todTint = uTimeOfDayColor.rgb;
    float todAlpha = uTimeOfDayColor.a;

    vec4 result = vec4(todTint, todAlpha);

    // Blend weather on top of ToD
    if (weatherAccum.a > 0.0) {
        result.rgb = mix(result.rgb, weatherAccum.rgb, weatherAccum.a);
        result.a = max(result.a, weatherAccum.a);
    }

    vFragmentColor = result;
}
