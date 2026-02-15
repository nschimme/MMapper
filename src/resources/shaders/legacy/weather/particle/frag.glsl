// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform vec4 uWeatherIntensities;
uniform vec4 uTimeOfDayColor;
in float vLife;
in float vType;
in float vDist;
in vec2 vVelScreen;
out vec4 vFragmentColor;

void main()
{
    if (vLife <= 0.0) discard;

    // Near-field fade to prevent "pop" or clipping into camera
    float nearFade = smoothstep(1.0, 3.0, vDist);
    if (nearFade <= 0.0) discard;

    float darkBoost = uTimeOfDayColor.a * 1.5;
    vec2 p = gl_PointCoord * 2.0 - 1.0;

    if (vType < 0.5) {
        // Rain: streak aligned with velocity
        float len = length(vVelScreen);
        vec2 dir = len > 0.0001 ? vVelScreen / len : vec2(0.0, 1.0);
        float distToLine = length(p - dot(p, dir) * dir);

        float streak = 1.0 - smoothstep(0.0, 0.15, distToLine);
        float alongLine = dot(p, dir);
        streak *= smoothstep(1.0, 0.7, abs(alongLine));

        float alpha = uWeatherIntensities.x * streak * nearFade * (0.6 + darkBoost);
        vec3 color = vec3(0.6 + darkBoost, 0.6 + darkBoost, 1.0);
        vFragmentColor = vec4(color, alpha);
    } else {
        // Snow: soft circle
        float dist = length(p);
        float flake = 1.0 - smoothstep(0.2, 0.6, dist);

        float alpha = uWeatherIntensities.y * flake * nearFade * (0.8 + darkBoost);
        vec3 color = vec3(1.0 + darkBoost, 1.0 + darkBoost, 1.1 + darkBoost);
        vFragmentColor = vec4(color, alpha);
    }
}
