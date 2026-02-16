// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

in float vHash;
in float vType;
in vec2 vLocalCoord;
in float vLocalMask;

uniform float uRainIntensity;
uniform float uSnowIntensity;
uniform vec4 uTimeOfDayColor;

out vec4 vFragmentColor;

void main()
{
    vec3 todTint = uTimeOfDayColor.rgb;
    vec4 pColor;
    if (vType == 0.0) { // Rain
        float streak = 1.0 - smoothstep(0.0, 0.15, abs(vLocalCoord.x - 0.5));
        pColor = vec4(0.6, 0.6, 1.0, uRainIntensity * streak * vLocalMask * 0.6);
    } else { // Snow
        float dist = distance(vLocalCoord, vec2(0.5));
        float flake = 1.0 - smoothstep(0.1, 0.2, dist);
        pColor = vec4(1.0, 1.0, 1.1, uSnowIntensity * flake * vLocalMask * 0.8);
    }

    if (pColor.a <= 0.0) {
        discard;
    }

    // Match the original sequential blending: mix(todTint, weatherColor.rgb, weatherColor.a)
    // Here we mix the particle color with the Time of Day tint.
    vFragmentColor.rgb = mix(todTint, pColor.rgb, pColor.a);
    vFragmentColor.a = pColor.a;
}
