// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

precision highp float;

in float vHash;
in float vType;
in vec2 vLocalCoord;
in float vLocalMask;
in vec2 vPos;

uniform float uRainIntensity;
uniform float uSnowIntensity;
uniform vec4 uTimeOfDayColor;

out vec4 vFragmentColor;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main()
{
    vec4 pColor;
    if (vType == 0.0) { // Rain
        float streak = 1.0 - smoothstep(0.0, 0.15, abs(vLocalCoord.x - 0.5));
        pColor = vec4(0.6, 0.6, 1.0, uRainIntensity * streak * vLocalMask * 0.6);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.2;
    } else { // Snow
        float dist = distance(vLocalCoord, vec2(0.5));
        float flake = 1.0 - smoothstep(0.1, 0.2, dist);
        pColor = vec4(1.0, 1.0, 1.1, uSnowIntensity * flake * vLocalMask * 0.8);
        // Emissive boost at night
        pColor.rgb += uTimeOfDayColor.a * 0.3;

        // Add spatial fading to match original's "sometimes fade and disappear" look
        // We use a moving grid cell hash like the original did implicitly
        float fade = hash(floor(vPos.xy * 4.0));
        pColor.a *= smoothstep(0.0, 0.1, fade - 0.04); // flake exists if hash > 0.04
    }

    if (pColor.a <= 0.0) {
        discard;
    }

    // Output raw color, let BlendModeEnum::MAX_ALPHA handle the untinted overlay.
    // MAX_ALPHA uses: RGB = src.rgb * src.a + dst.rgb * (1 - src.a)
    // This correctly overlays bright particles on tinted background.
    vFragmentColor = pColor;
}
