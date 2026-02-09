// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform float uDevicePixelRatio;
uniform vec3 uMapCenter;
uniform float uBaseSize;

struct IconMetrics
{
    // xy: Default size in world units or pixels (used if instance size is zero)
    // zw: Relative anchor offset (-0.5 is centered, 0.0 is top-left)
    vec4 sizeAnchor;
    uint flags;
    uint padding[3];
};

// Binding point 2
layout(std140, binding = 2) uniform IconMetricsBlock
{
    IconMetrics uIconMetrics[256];
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in uint aColor;
layout(location = 2) in ivec2 aSize;
layout(location = 3) in uint aPacked;

// Flags (from IconMetrics)
const uint FLAG_SCREEN_SPACE = 1u;
const uint FLAG_FIXED_SIZE = 2u;
const uint FLAG_DISTANCE_SCALE = 4u;
const uint FLAG_CLAMP_TO_EDGE = 8u;
const uint FLAG_AUTO_ROTATE = 16u;

out vec4 vColor;
out vec3 vTexCoord;

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint rotationRaw = aPacked & 0x1FFu;
    uint iconIndex = (aPacked >> 9u) & 0xFFu;

    IconMetrics metrics = uIconMetrics[iconIndex];
    uint flags = metrics.flags;

    float rotation = radians(float(rotationRaw));

    vColor = vec4(
        float(aColor & 0xFFu) / 255.0,
        float((aColor >> 8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );

    vec2 size = vec2(aSize);
    if (size.x == 0.0 && size.y == 0.0) {
        size = metrics.sizeAnchor.xy;
    }

    if ((flags & FLAG_SCREEN_SPACE) == 0u && (flags & FLAG_FIXED_SIZE) == 0u) {
        size /= 256.0;

        if ((flags & FLAG_DISTANCE_SCALE) != 0u) {
            float dist = length(aBase.xy - uMapCenter.xy);
            size *= clamp(1.0 - dist / uBaseSize, 0.5, 1.0);
        }
    }
    vec2 anchor = metrics.sizeAnchor.zw;

    const vec2 quad[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];
    vTexCoord = vec3(corner, float(iconIndex));

    vec2 posOffset = (corner + anchor) * size;

    if ((flags & FLAG_FIXED_SIZE) != 0u) {
        posOffset *= uDevicePixelRatio;
    }

    // Rotation
    float s = sin(rotation);
    float c = cos(rotation);
    posOffset = vec2(posOffset.x * c - posOffset.y * s, posOffset.x * s + posOffset.y * c);

    vec2 viewportSize = max(vec2(uPhysViewport.zw), vec2(1.0));
    vec2 viewportOffset = vec2(uPhysViewport.xy);

    if ((flags & FLAG_SCREEN_SPACE) != 0u) {
        // aBase is in pixels from top-left.
        vec2 pixelPos = aBase.xy + posOffset;

        // Convert back to NDC, handling Y-flip
        vec2 ndc = (pixelPos - viewportOffset) * 2.0 / viewportSize - 1.0;
        ndc.y = -ndc.y; // Y-flip for screen space
        gl_Position = vec4(ndc, 0.0, 1.0);
    } else if ((flags & FLAG_FIXED_SIZE) != 0u || (flags & FLAG_CLAMP_TO_EDGE) != 0u) {
        // World-space anchor, but fixed pixel size or clamped to edge.
        vec4 ndcPos = uMVP3D * vec4(aBase, 1.0);

        // Check if point is behind camera
        if (ndcPos.w < 1e-3) {
            if ((flags & FLAG_CLAMP_TO_EDGE) != 0u) {
                // For off-screen indicators, we can approximate by flipping
                ndcPos.xy = -normalize(ndcPos.xy) * 2.0;
            } else {
                gl_Position = ignored;
                return;
            }
        }

        ndcPos /= ndcPos.w;

        bool isOffScreen = any(greaterThan(abs(ndcPos.xy), vec2(1.0)));

        if (isOffScreen && (flags & FLAG_CLAMP_TO_EDGE) != 0u) {
            vec2 margin = 40.0 / viewportSize; // 20px margin in pixels -> NDC
            float scale = min((1.0 - margin.x) / abs(ndcPos.x), (1.0 - margin.y) / abs(ndcPos.y));
            ndcPos.xy *= scale;

            if ((flags & FLAG_AUTO_ROTATE) != 0u) {
                // Point arrow from center to object
                rotation = atan(ndcPos.y, ndcPos.x);
                // Re-calculate posOffset with new rotation
                float s = sin(rotation);
                float c = cos(rotation);
                posOffset = (corner + anchor) * size;
                if ((flags & FLAG_FIXED_SIZE) != 0u)
                    posOffset *= uDevicePixelRatio;
                posOffset = vec2(posOffset.x * c - posOffset.y * s, posOffset.x * s + posOffset.y * c);
            }
        } else if (isOffScreen && (flags & FLAG_FIXED_SIZE) != 0u) {
            gl_Position = ignored;
            return;
        }

        vec2 pixelPos = (ndcPos.xy * 0.5 + 0.5) * viewportSize + viewportOffset;

        pixelPos = floor(pixelPos + 0.5);
        pixelPos += posOffset;

        ndcPos.xy = (pixelPos - viewportOffset) * 2.0 / viewportSize - 1.0;
        gl_Position = vec4(ndcPos.xy, 0.0, 1.0); // Indicators usually on top
    } else {
        // Full world-space quad (like room selections)
        vec3 worldPos = aBase + vec3(posOffset, 0.0);
        gl_Position = uMVP3D * vec4(worldPos, 1.0);
    }
}
