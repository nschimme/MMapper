// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform ivec2 uFontTexSize;

struct GlyphMetrics {
    ivec4 uvRect;
};

// Binding point 1
layout(std140) uniform GlyphMetricsBlock {
    GlyphMetrics uGlyphMetrics[1024];
};

uniform NamedColorsBlock {
    vec4 uNamedColors[MAX_NAMED_COLORS];
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in vec4 aColor;
layout(location = 2) in ivec4 aRect;   // offsetX, offsetY, sizeW, sizeH
layout(location = 3) in uint aPacked;  // glyphId (10), rotation (10), flags (6), namedColor (6)

const uint FLAG_ITALICS = 1u;
const uint FLAG_NAMED_COLOR = 2u;

out vec4 vColor;
out vec2 vTexCoord;

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint glyphId = aPacked & 0x3FFu;
    int rotationInt = int((aPacked >> 10u) & 0x3FFu);
    if (rotationInt >= 512) rotationInt -= 1024;
    float rotation = radians(float(rotationInt));
    uint flags = (aPacked >> 20u) & 0x3Fu;
    uint namedColorIndex = (aPacked >> 26u) & 0x3Fu;

    vec4 namedCol = uNamedColors[namedColorIndex % uint(MAX_NAMED_COLORS)];
    // The named color's alpha is modulated by the instance alpha
    vec4 finalNamedCol = vec4(namedCol.rgb, namedCol.a * aColor.a);

    vColor = mix(aColor, finalNamedCol, float((flags & FLAG_NAMED_COLOR) != 0u));

    const vec2 quad[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];

    ivec4 uvRect = uGlyphMetrics[glyphId].uvRect;

    // Improved sampling for solid blocks (ID 256 = underline, 257 = background)
    // We sample from the center of the white block to avoid edge bleeding.
    vec2 uv;
    if (glyphId >= 256u) {
        // For stretched solid quads, we sample from the center of the 4x4 block
        // to ensure linear filtering always picks up opaque white.
        uv = vec2(uvRect.xy) + 2.0;
    } else {
        uv = vec2(uvRect.xy) + corner * vec2(uvRect.zw);
    }
    vTexCoord = uv / vec2(uFontTexSize);

    vec2 posPixels = vec2(aRect.xy) + corner * vec2(aRect.zw);

    // Italics
    if ((flags & FLAG_ITALICS) != 0u) {
        posPixels.x += (posPixels.y / 6.0);
    }

    // Rotation
    float s = sin(rotation);
    float c = cos(rotation);
    posPixels = vec2(posPixels.x * c - posPixels.y * s, posPixels.x * s + posPixels.y * c);

    vec4 ndcPos = uMVP3D * vec4(aBase, 1); /* 3D transform */

    // Clipping tests
    if (any(greaterThan(abs(ndcPos.xyz), vec3(1.5 * abs(ndcPos.w)))) || (abs(ndcPos.w) < 1e-3)) {
        gl_Position = ignored;
        return;
    }

    ndcPos /= ndcPos.w;

    // Convert NDC to physical pixels
    vec2 viewportSize = vec2(uPhysViewport.zw);
    vec2 viewportOffset = vec2(uPhysViewport.xy);
    vec2 pixelPos = (ndcPos.xy * 0.5 + 0.5) * viewportSize + viewportOffset;

    // Snap to pixels (anti-flicker)
    pixelPos = floor(pixelPos + 0.5);

    // Add per-glyph offset in pixels
    pixelPos += posPixels;

    // Convert back to NDC
    ndcPos.xy = ((pixelPos - viewportOffset) / viewportSize) * 2.0 - 1.0;

    gl_Position = ndcPos;
}
