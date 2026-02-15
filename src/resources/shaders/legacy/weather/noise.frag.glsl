// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform vec3 uPlayerPos;
uniform float uTime;
uniform vec4 uWeatherIntensities; // x: rain, y: snow, z: clouds, w: fog

in vec2 vWorldPos;
in vec2 vTexCoord;
out vec4 vFragmentColor;

// Simple hash and noise functions
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }

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
    vec2 worldPos = vWorldPos;

    // Channel R: Fog noise
    // Channel G: Cloud noise
    float fog = 0.0;
    if (uWeatherIntensities.w > 0.0) {
        fog = fbm(worldPos * 0.15 + uTime * 0.1);
    }

    float clouds = 0.0;
    if (uWeatherIntensities.z > 0.0) {
        clouds = fbm(worldPos * 0.06 - uTime * 0.03);
    }

    vFragmentColor = vec4(fog, clouds, 0.0, 1.0);
}
