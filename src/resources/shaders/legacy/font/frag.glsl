// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform sampler2D uFontTexture;

in vec4 vColor;
in vec2 vTexCoord;

out vec4 vFragmentColor;

void main()
{
    // Sample single-channel (Red) distance field texture value
    float dist = texture(uFontTexture, vTexCoord).r;

    // Calculate screen-space derivative for dynamic anti-aliasing at any zoom level
    float smoothing = clamp(fwidth(dist), 0.01, 0.25);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);

    vFragmentColor = vec4(vColor.rgb, vColor.a * alpha);
}
