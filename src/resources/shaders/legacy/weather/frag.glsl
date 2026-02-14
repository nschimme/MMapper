// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uInvViewProj;
uniform vec4 uPlayerPos;
uniform float uZScale;
uniform ivec4 uPhysViewport;
uniform float uTime;
uniform float uRainIntensity;
uniform float uSnowIntensity;
uniform float uCloudsIntensity;
uniform float uFogIntensity;
uniform vec4 uTimeOfDayColor;

out vec4 vFragmentColor;

// Simple hash and noise functions
float hash(float n)
{
    return fract(sin(n) * 43758.5453123);
}
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
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

    // Fog
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.1 + uTime * 0.05);
        weatherColor += vec4(0.8, 0.8, 0.9, uFogIntensity * n * localMask);
    }

    // Clouds
    if (uCloudsIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.05 - uTime * 0.02);
        weatherColor += vec4(0.7, 0.7, 0.8, uCloudsIntensity * n * localMask * 0.6);
    }

    // Rain
    if (uRainIntensity > 0.0) {
        vec2 rv = worldPos.xy * vec2(2.0, 0.5);
        rv.y += uTime * 15.0;
        float r = hash(floor(rv.x * 20.0));
        float rain = step(0.95, hash(vec2(floor(rv.x * 20.0), floor(rv.y))));
        if (rain > 0.0) {
            weatherColor += vec4(0.5, 0.5, 1.0, uRainIntensity * localMask * 0.4);
        }
    }

    // Snow
    if (uSnowIntensity > 0.0) {
        vec2 sv = worldPos.xy * 2.0 + uTime * vec2(0.5, 1.0);
        float n = noise(sv);
        if (n > 0.8) {
            weatherColor += vec4(1.0, 1.0, 1.0, uSnowIntensity * localMask * (n - 0.8) * 4.0);
        }
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
