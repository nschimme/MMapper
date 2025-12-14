// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

uniform float uTime;
in vec2 vTexCoord;

out vec4 vFragmentColor;

// Simple noise function
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / 400.0;
    float noise = rand(uv + uTime);
    vFragmentColor = vec4(0.5, 0.5, 0.5, noise * 0.5);
}
