// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

in float vType;
in float vLife;
in vec2 vLocalCoord;
in float vLocalMask;

uniform float uRainIntensity;
uniform float uSnowIntensity;
uniform vec4 uTimeOfDayColor;

out vec4 vFragmentColor;

void main()
{
    vec4 pColor;
    float lifeFade = smoothstep(0.0, 0.15, vLife) * smoothstep(1.0, 0.85, vLife);

    if (vType == 0.0) { // Rain
        float streak = 1.0 - smoothstep(0.0, 0.15, abs(vLocalCoord.x - 0.5));
        pColor = vec4(0.6, 0.6, 1.0, uRainIntensity * streak * vLocalMask * 0.6 * lifeFade);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.2;
    } else { // Snow
        float dist = distance(vLocalCoord, vec2(0.5));
        float flake = 1.0 - smoothstep(0.1, 0.2, dist);
        pColor = vec4(1.0, 1.0, 1.1, uSnowIntensity * flake * vLocalMask * 0.8 * lifeFade);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.3;

        // Spatial fading is now handled in simulation (vLife will be small in holes)
    }

    if (pColor.a <= 0.0) {
        discard;
    }

    vFragmentColor = pColor;
}
