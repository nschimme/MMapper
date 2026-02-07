#version 330 core

layout(location = 0) in vec4 aColor;
layout(location = 1) in vec4 aRect;
layout(location = 2) in uint aPacked;
layout(location = 3) in vec3 aBase;

out vec4 vColor;
out vec2 vTexCoord;

struct GlyphMetrics {
    ivec4 uvRect;
    ivec4 metrics;
};

// Binding points are assigned automatically by Legacy::Functions::applyDefaultUniformBlockBindings
// mapping SharedVboEnum values to binding points.
// NamedColorsBlock = 0, GlyphMetricsBlock = 1.
layout(std140) uniform GlyphMetricsBlock {
    GlyphMetrics glyphMetrics[1024];
};

layout(std140) uniform NamedColorsBlock {
    vec4 namedColors[256];
};

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform ivec2 uFontTexSize;

const uint FLAG_ITALIC = 1u;
const uint FLAG_NAMED_COLOR = 2u;
const uint FLAG_USE_EXPLICIT_RECT = 4u;

const uint GLYPH_ID_UNDERLINE = 256u;
const uint GLYPH_ID_BACKGROUND = 257u;

const vec2 positions[4] = vec2[](
    vec2(0.0, 0.0), // Bottom Left
    vec2(1.0, 0.0), // Bottom Right
    vec2(1.0, 1.0), // Top Right
    vec2(0.0, 1.0)  // Top Left
);

void main()
{
    uint glyphId = aPacked & 0x3FFu;
    int rotationInt = int((aPacked >> 10u) & 0x3FFu);
    if (rotationInt >= 512) rotationInt -= 1024;
    float rotation = float(rotationInt) * (3.14159265358979323846 / 180.0);
    uint flags = (aPacked >> 20u) & 0x3Fu;
    uint namedColorIndex = (aPacked >> 26u) & 0x3Fu;

    vec2 pos = positions[gl_VertexID];

    uint id = glyphId;
    bool useExplicitRect = (flags & FLAG_USE_EXPLICIT_RECT) != 0u;

    ivec4 uvRect = glyphMetrics[id].uvRect;
    ivec4 m = glyphMetrics[id].metrics;

    float w = useExplicitRect ? aRect.z : float(m.x);
    float h = useExplicitRect ? aRect.w : float(m.y);

    vec2 localPos;
    localPos.x = aRect.x + float(m.z) + pos.x * w;
    localPos.y = -(aRect.y + float(m.w) + (1.0 - pos.y) * h);

    if ((flags & FLAG_ITALIC) != 0u) {
        localPos.x += pos.y * 0.25 * h;
    }

    vec2 rotatedPos;
    rotatedPos.x = localPos.x * cos(rotation) - localPos.y * sin(rotation);
    rotatedPos.y = localPos.x * sin(rotation) + localPos.y * cos(rotation);

    vec4 screenPos = uMVP3D * vec4(aBase, 1.0);
    screenPos.xyz /= screenPos.w;
    // viewport.zw is the size of the viewport.
    screenPos.xy = (screenPos.xy * 0.5 + 0.5) * vec2(uPhysViewport.zw);

    screenPos.xy += rotatedPos;

    // Anti-flicker: Snap to physical pixels
    screenPos.xy = floor(screenPos.xy + 0.5);

    gl_Position = vec4((screenPos.xy / vec2(uPhysViewport.zw) - 0.5) * 2.0, screenPos.z, 1.0);

    vColor = aColor;
    if ((flags & FLAG_NAMED_COLOR) != 0u) {
        vColor.rgb = namedColors[namedColorIndex].rgb;
    }

    vTexCoord.x = (float(uvRect.x) + pos.x * float(uvRect.z)) / float(uFontTexSize.x);
    vTexCoord.y = (float(uvRect.y) + pos.y * float(uvRect.w)) / float(uFontTexSize.y);
}
