// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

// The font atlas is a single RGBA texture holding two kinds of glyphs:
// - monochrome glyphs: RGB is opaque white, and A carries the signed
//   distance field value (tinted by vColor).
// - color glyphs (e.g. emoji): RGBA holds the actual glyph color, sampled
//   as-is and only modulated by vColor.a for fading.
uniform sampler2D uFontTexture;

in vec4 vColor;
in vec2 vTexCoord;
flat in float vIsColor;

out vec4 vFragmentColor;

void main()
{
    vec4 texel = texture(uFontTexture, vTexCoord);

    if (vIsColor > 0.5) {
        vFragmentColor = vec4(texel.rgb, texel.a * vColor.a);
        return;
    }

    // Sample distance field value from the alpha channel.
    float dist = texel.a;

    // Calculate screen-space derivative for dynamic anti-aliasing at any zoom level
    float smoothing = clamp(fwidth(dist), 0.01, 0.25);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);

    vFragmentColor = vec4(vColor.rgb, vColor.a * alpha);
}
