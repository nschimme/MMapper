// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform float uDevicePixelRatio;

struct GlyphMetrics
{
    // xy: uvOffset (normalized 0.0 to 1.0)
    // zw: uvSize (normalized 0.0 to 1.0)
    vec4 uvRect;

    // xy: pixelOffset relative to cursor baseline (pre-scaled by DPR)
    // zw: pixelSize (pre-scaled by DPR)
    vec4 posRect;
};

// Binding point 1
layout(std140, binding = 1) uniform GlyphMetricsBlock
{
    GlyphMetrics uGlyphMetrics[512];
};

layout(std140, binding = 0) uniform NamedColorsBlock
{
    vec4 uNamedColors[MAX_NAMED_COLORS];
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in vec4 aColor;
layout(location = 2) in int aOffsetX;
layout(location = 3) in uint aPacked1;
layout(location = 4) in uint aPackedRest;

const uint FLAG_ITALICS = 1u;
const uint FLAG_NAMED_COLOR = 2u;

out vec4 vColor;
out vec2 vTexCoord;

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint glyphId = aPacked1 & 0x3FFu;
    uint flags = (aPacked1 >> 10u) & 0x3Fu;
    float rotation = 0.0;
    uint namedColorIndex = 0u;

    if (glyphId < 256u) {
        rotation = radians(float(aPackedRest & 0x1FFu));
        namedColorIndex = (aPackedRest >> 9u) & 0x3Fu;
    } else {
        namedColorIndex = (aPackedRest >> 26u) & 0x3Fu;
    }

    vec4 namedCol = uNamedColors[namedColorIndex % uint(MAX_NAMED_COLORS)];
    // The named color's alpha is modulated by the instance alpha
    vec4 finalNamedCol = vec4(namedCol.rgb, namedCol.a * aColor.a);

    vColor = mix(aColor, finalNamedCol, float((flags & FLAG_NAMED_COLOR) != 0u));

    const vec2 quad[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];

    GlyphMetrics metrics = uGlyphMetrics[glyphId % 512u];
    vTexCoord = metrics.uvRect.xy + corner * metrics.uvRect.zw;

    vec2 posPixels;
    if (glyphId >= 256u) {
        // Synthetic: packRest = offsetY (8), sizeW (10), sizeH (8), namedColorIndex (6)
        int offsetY = int(aPackedRest << 24u) >> 24u;
        int sizeW = int((aPackedRest >> 8u) & 0x3FFu);
        int sizeH = int(aPackedRest << 6u) >> 24u; // sizeH starts at bit 18, length 8. 32 - 18 - 8 = 6 bits shift.

        posPixels = (vec2(float(aOffsetX), float(offsetY)) + corner * vec2(float(sizeW), float(sizeH)));
    } else {
        // Normal character: use UBO posRect + aOffsetX (cursorX).
        vec4 posRect = metrics.posRect;
        posPixels = (posRect.xy + vec2(float(aOffsetX), 0.0) + corner * posRect.zw);
    }

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
    vec2 viewportSize = max(vec2(uPhysViewport.zw), vec2(1.0));
    vec2 viewportOffset = vec2(uPhysViewport.xy);
    vec2 pixelPos = (ndcPos.xy * 0.5 + 0.5) * viewportSize + viewportOffset;

    // Snap to pixels (anti-flicker)
    pixelPos = floor(pixelPos + 0.5);

    // Add per-glyph offset in pixels
    pixelPos += posPixels;

    // Convert back to NDC
    ndcPos.xy = (pixelPos - viewportOffset) * 2.0 / viewportSize - 1.0;

    gl_Position = ndcPos;
}
