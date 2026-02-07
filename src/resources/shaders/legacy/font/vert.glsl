// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform ivec2 uFontTexSize;

uniform NamedColorsBlock {
    vec4 uNamedColors[MAX_NAMED_COLORS];
};

struct GlyphMetrics {
    ivec4 uvRect;  // uvX, uvY, uvW, uvH
    ivec4 metrics; // width, height, xoffset, yoffset
};

uniform GlyphMetricsBlock {
    GlyphMetrics uGlyphs[512]; // 256 latin-1 + 256 specials
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in uint aColor;
layout(location = 2) in ivec4 aRect;   // offsetX, offsetY, sizeW, sizeH
layout(location = 3) in uint aPacked;  // glyphId (10), rotation (10), flags (6), namedColor (6)

const uint FLAG_ITALICS = 1u;
const uint FLAG_NAMED_COLOR = 2u;
const uint FLAG_USE_EXPLICIT_RECT = 4u;

out vec4 vColor;
out vec2 vTexCoord;

// [0, 1]^2 to pixels
vec2 convertScreen01toPhysPixels(vec2 pos)
{
    return pos * vec2(uPhysViewport.zw) + vec2(uPhysViewport.xy);
}

vec2 convertPhysPixelsToScreen01(vec2 pixels)
{
    return (pixels - vec2(uPhysViewport.xy)) / vec2(uPhysViewport.zw);
}

vec2 anti_flicker(vec2 pos)
{
    return convertPhysPixelsToScreen01(floor(convertScreen01toPhysPixels(pos)));
}

// [-1, 1]^2 to [0, 1]^2
vec2 convertNDCtoScreenSpace(vec2 pos)
{
    return pos * 0.5 + 0.5;
}

// [0, 1]^2 to [-1, 1]^2
vec2 convertScreen01toNdcClip(vec2 pos)
{
    pos = pos * 2.0 - 1.0;
    return pos;
}

vec4 unpackRGBA(uint c)
{
    return vec4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8u) & 0xFFu) / 255.0,
        float((c >> 16u) & 0xFFu) / 255.0,
        float((c >> 24u) & 0xFFu) / 255.0
    );
}

vec2 addPerVertexOffset(vec2 wordOriginNdc, vec2 glyphOffsetPixels)
{
    vec2 wordOriginScreen = anti_flicker(convertNDCtoScreenSpace(wordOriginNdc));
    return convertScreen01toNdcClip(wordOriginScreen + convertPhysPixelsToScreen01(glyphOffsetPixels));
}

// NOTE:
// 1. Returning identical coordinates will yield degenerate
//    triangles, so no fragments will be generated.
// 2. Returning a value that is outside the clip region
//    might help some implementations bail out sooner.
//    (hint: 2 is outside the clip region [-1, 1])
const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint glyphId = aPacked & 0x3FFu;
    int rotation = int(aPacked >> 10u) & 0x3FF;
    if (rotation >= 512) rotation -= 1024; // Sign extend 10 bits

    uint flags = (aPacked >> 20u) & 0x3Fu;
    uint namedColorIndex = (aPacked >> 26u) & 0x3Fu;

    GlyphMetrics g = uGlyphs[glyphId % 512u];

    // Branchless color selection
    vec4 unpackedCol = unpackRGBA(aColor);
    vec4 namedCol = uNamedColors[namedColorIndex % uint(MAX_NAMED_COLORS)];

    // The named color's alpha is modulated by the instance alpha (e.g. for background opacity)
    vec4 finalNamedCol = vec4(namedCol.rgb, namedCol.a * unpackedCol.a);

    vColor = mix(unpackedCol, finalNamedCol, float((flags & FLAG_NAMED_COLOR) != 0u));

    const vec2[4] quad = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];

    vTexCoord = (vec2(g.uvRect.xy) + corner * vec2(g.uvRect.zw)) / vec2(uFontTexSize);

    bool useExplicit = (flags & FLAG_USE_EXPLICIT_RECT) != 0u;

    // aRect = offsetX, offsetY, sizeW, sizeH
    // g.metrics = width, height, xoffset, yoffset
    vec2 size = mix(vec2(g.metrics.xy), vec2(aRect.zw), float(useExplicit));
    vec2 offset = mix(vec2(aRect.x + g.metrics.z, g.metrics.w), vec2(aRect.xy), float(useExplicit));

    vec2 posPixels = offset + corner * size;

    // Branchless italics
    posPixels.x += (posPixels.y / 6.0) * float((flags & FLAG_ITALICS) != 0u);

    // Branchless rotation
    float rad = radians(float(rotation));
    float s = sin(rad);
    float c = cos(rad);
    posPixels = vec2(posPixels.x * c - posPixels.y * s, posPixels.x * s + posPixels.y * c);

    vec4 pos = uMVP3D * vec4(aBase, 1); /* 3D transform */

    // Clipping tests
    bool isInvalid = any(greaterThan(abs(pos.xyz), vec3(1.5 * abs(pos.w)))) || (abs(pos.w) < 1e-3);
    if (isInvalid) {
        gl_Position = ignored;
        return;
    }

    // convert from clip space to normalized device coordinates (NDC)
    pos /= pos.w;

    pos.xy = addPerVertexOffset(pos.xy, posPixels);

    // Note: We're not using the built-in MVP matrix.
    gl_Position = pos;
}
