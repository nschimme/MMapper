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
layout(std140) uniform GlyphMetricsBlock
{
    GlyphMetrics uGlyphMetrics[512];
};

layout(std140) uniform NamedColorsBlock
{
    vec4 uNamedColors[MAX_NAMED_COLORS];
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in vec4 aColor;
layout(location = 2) in ivec4 aRect;  // offsetX, offsetY, sizeW, sizeH
layout(location = 3) in uint aPacked; // glyphId (10), rotation (10), flags (6), namedColor (6)

const uint FLAG_ITALICS = 1u;
const uint FLAG_NAMED_COLOR = 2u;

out vec4 vColor;
out vec2 vTexCoord;

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint glyphId = aPacked & 0x7FFu; // 11 bits (up to 2047), but UBO limited to 512
    float rotation = radians(float((aPacked >> 11u) & 0x1FFu)); // 9 bits (0-511)
    uint flags = (aPacked >> 20u) & 0x3Fu;
    uint namedColorIndex = (aPacked >> 26u) & 0x3Fu;

    vec4 namedCol = uNamedColors[namedColorIndex % uint(MAX_NAMED_COLORS)];
    // The named color's alpha is modulated by the instance alpha
    vec4 finalNamedCol = vec4(namedCol.rgb, namedCol.a * aColor.a);

    vColor = mix(aColor, finalNamedCol, float((flags & FLAG_NAMED_COLOR) != 0u));

    const vec2 quad[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];

    GlyphMetrics metrics = uGlyphMetrics[glyphId];
    vTexCoord = metrics.uvRect.xy + corner * metrics.uvRect.zw;

    vec2 posPixels;
    if (glyphId >= 256u) {
        // Synthetic: use full aRect.
        // For 2D labels, these might need scaling?
        // But for synthetic quads (bg/underline), they are usually already in pixels.
        posPixels = (vec2(aRect.xy) + corner * vec2(aRect.zw));
    } else {
        // Normal character: use UBO posRect + aRect.x (cursorX).
        // Font metrics in UBO are already scaled to physical pixels during loading.
        vec4 posRect = metrics.posRect;
        posPixels = (posRect.xy + vec2(aRect.x, 0) + corner * posRect.zw);
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
    vec2 viewportSize = vec2(uPhysViewport.zw);
    vec2 viewportOffset = vec2(uPhysViewport.xy);
    vec2 pixelPos = (ndcPos.xy * 0.5 + 0.5) * viewportSize + viewportOffset;

    // Snap to pixels (anti-flicker)
    pixelPos = floor(pixelPos + 0.5);

    // Add per-glyph offset in pixels
    pixelPos += posPixels;

    // Convert back to NDC
    ndcPos.xy = (pixelPos - viewportOffset) * 2.0 / vec2(uPhysViewport.zw) - 1.0;

    gl_Position = ndcPos;
}
