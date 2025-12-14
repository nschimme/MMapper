// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;
uniform float uLineWidth;

// Per-instance attributes
layout(location = 0) in vec3 aFrom;
layout(location = 1) in vec3 aTo;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aStipple; // x: factor, y: pattern

// Per-vertex attribute (for the quad)
layout(location = 4) in vec2 aCorner; // (-1,-1), (1,-1), (-1,1), (1,1)

out vec4 vColor;
out vec2 vStipple;
out float vStipplePos;
out float vLineLength;

vec2 worldToScreen(vec3 worldPos) {
    vec4 clipPos = uMVP3D * vec4(worldPos, 1.0);
    vec2 ndc = clipPos.xy / clipPos.w;
    return (ndc * 0.5 + 0.5) * uPhysViewport.zw;
}

void main() {
    vColor = aColor;
    vStipple = aStipple;

    vec2 screenFrom = worldToScreen(aFrom);
    vec2 screenTo = worldToScreen(aTo);

    vec2 lineVec = screenTo - screenFrom;
    float len = length(lineVec);
    vLineLength = len;
    vec2 lineDir = (len > 0.0) ? lineVec / len : vec2(1.0, 0.0);

    vec2 normal = vec2(-lineDir.y, lineDir.x);
    float halfWidth = uLineWidth * 0.5;

    vec2 cornerOffset = aCorner * halfWidth;
    vec2 offset = normal * cornerOffset.y;

    vStipplePos = cornerOffset.x;

    vec2 currentPos = (cornerOffset.x > 0.0) ? screenTo : screenFrom;
    currentPos += offset;

    // Convert back to NDC
    vec2 screen01 = currentPos / uPhysViewport.zw;
    vec2 ndc = screen01 * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
}
