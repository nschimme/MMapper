// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform float uRainIntensity;
uniform float uSnowIntensity;
uniform vec4 uTimeOfDayColor;
in float vLife;
in float vType;
in float vDist;
in vec2 vVelScreen;
out vec4 vFragmentColor;

void main()
{
    if (vLife <= 0.0) discard;

    // Near-field fade
    float nearFade = smoothstep(1.0, 3.0, vDist);
    if (nearFade <= 0.0) discard;

    float darkBoost = uTimeOfDayColor.a * 1.5;
    vec2 p = gl_PointCoord * 2.0 - 1.0;

    if (vType < 0.5) { // Rain
        float len = length(vVelScreen);
        vec2 dir = len > 0.0001 ? vVelScreen / len : vec2(0.0, 1.0);
        float streak = 1.0 - smoothstep(0.0, 0.15, abs(p.x * dir.y - p.y * dir.x)); // distance to line
        streak *= smoothstep(1.0, 0.7, abs(dot(p, dir))); // fade ends

        float alpha = uRainIntensity * streak * nearFade * (0.6 + darkBoost);
        vec3 color = vec3(0.6 + darkBoost, 0.6 + darkBoost, 1.0);
        vFragmentColor = vec4(color, alpha);
    } else { // Snow
        float dist = length(p);
        float flake = 1.0 - smoothstep(0.2, 0.6, dist);
        float alpha = uSnowIntensity * flake * nearFade * (0.8 + darkBoost);
        vec3 color = vec3(1.0 + darkBoost, 1.0 + darkBoost, 1.1 + darkBoost);
        vFragmentColor = vec4(color, alpha);
    }
}
