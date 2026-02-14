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
    float localMask = smoothstep(11.0, 7.0, distToPlayer);

    vec4 weatherColor = vec4(0.0);

    // Fog: soft drifting noise
    if (uFogIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.15 + uTime * 0.1);
        weatherColor += vec4(0.8, 0.8, 0.85, uFogIntensity * n * localMask * 0.8);
    }

    // Clouds: puffy high-contrast noise
    if (uCloudsIntensity > 0.0) {
        float n = fbm(worldPos.xy * 0.08 - uTime * 0.05);
        float puffy = smoothstep(0.4, 0.6, n);
        weatherColor += vec4(0.9, 0.9, 1.0, uCloudsIntensity * puffy * localMask * 0.5);
    }

    // Rain: falling streaks
    if (uRainIntensity > 0.0) {
        vec2 gv = worldPos.xy * vec2(4.0, 1.0);
        gv.y += uTime * 20.0;
        vec2 id = floor(gv);
        float h = hash(id.x);
        gv.y += h * 10.0; // random vertical offset
        float rain = step(0.98, hash(floor(gv)));
        if (rain > 0.0) {
            weatherColor += vec4(0.6, 0.6, 1.0, uRainIntensity * localMask * 0.5);
        }
    }

    // Snow: falling flakes
    if (uSnowIntensity > 0.0) {
        vec2 sv = worldPos.xy * 3.0;
        sv.y += uTime * 3.0;
        sv.x += sin(uTime + sv.y) * 0.5; // slight horizontal drift
        vec2 id = floor(sv);
        float h = hash(id);
        if (h > 0.97) {
            // Small bright flake
            weatherColor += vec4(1.0, 1.0, 1.0, uSnowIntensity * localMask);
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
