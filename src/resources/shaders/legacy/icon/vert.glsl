// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform float uDevicePixelRatio;

struct IconMetrics
{
    vec4 uvRect;
    vec4 sizeAnchor; // x,y = size, z,w = anchor offset
};

// Binding point 2
layout(std140) uniform IconMetricsBlock
{
    IconMetrics uIconMetrics[256];
};

layout(location = 0) in vec3 aBase;
layout(location = 1) in uint aColor;
layout(location = 2) in ivec2 aSize;
layout(location = 3) in uint aPacked;

// Flags (bits 17+)
const uint FLAG_SCREEN_SPACE = 1u;
const uint FLAG_FIXED_SIZE = 2u;

out vec4 vColor;
out vec3 vTexCoord;

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    uint rotationRaw = aPacked & 0x1FFu;
    uint iconIndex = (aPacked >> 9u) & 0xFFu;
    uint flags = (aPacked >> 17u);

    float rotation = radians(float(rotationRaw));

    vColor = vec4(
        float(aColor & 0xFFu) / 255.0,
        float((aColor >> 8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );

    IconMetrics metrics = uIconMetrics[iconIndex];
    vec2 size = vec2(aSize);
    if ((flags & FLAG_SCREEN_SPACE) == 0u && (flags & FLAG_FIXED_SIZE) == 0u) {
        size /= 256.0;
    }
    vec2 anchor = metrics.sizeAnchor.zw;

    const vec2 quad[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    vec2 corner = quad[gl_VertexID];
    vTexCoord = vec3(metrics.uvRect.xy + corner * metrics.uvRect.zw, float(iconIndex));

    vec2 posOffset = (corner + anchor) * size;

    if ((flags & FLAG_FIXED_SIZE) != 0u) {
        posOffset *= uDevicePixelRatio;
    }

    // Rotation
    float s = sin(rotation);
    float c = cos(rotation);
    posOffset = vec2(posOffset.x * c - posOffset.y * s, posOffset.x * s + posOffset.y * c);

    vec2 viewportSize = vec2(uPhysViewport.zw);
    vec2 viewportOffset = vec2(uPhysViewport.xy);

    if ((flags & FLAG_SCREEN_SPACE) != 0u) {
        // aBase is in pixels from top-left.
        vec2 pixelPos = aBase.xy + posOffset;

        // Convert back to NDC, handling Y-flip
        vec2 ndc = (pixelPos - viewportOffset) * 2.0 / viewportSize - 1.0;
        ndc.y = -ndc.y; // Y-flip for screen space
        gl_Position = vec4(ndc, 0.0, 1.0);
    } else if ((flags & FLAG_FIXED_SIZE) != 0u) {
        // World-space anchor, but fixed pixel size (like beacons)
        vec4 ndcPos = uMVP3D * vec4(aBase, 1.0);

        if (any(greaterThan(abs(ndcPos.xyz), vec3(1.5 * abs(ndcPos.w)))) || (abs(ndcPos.w) < 1e-3)) {
            gl_Position = ignored;
            return;
        }

        ndcPos /= ndcPos.w;

        vec2 pixelPos = (ndcPos.xy * 0.5 + 0.5) * viewportSize + viewportOffset;

        pixelPos = floor(pixelPos + 0.5);
        pixelPos += posOffset;

        ndcPos.xy = (pixelPos - viewportOffset) * 2.0 / viewportSize - 1.0;
        gl_Position = ndcPos;
    } else {
        // Full world-space quad (like room selections)
        vec3 worldPos = aBase + vec3(posOffset, 0.0);
        gl_Position = uMVP3D * vec4(worldPos, 1.0);
    }
}
