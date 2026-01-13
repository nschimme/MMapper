// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

uniform mat4 uMVP3D;
uniform ivec4 uPhysViewport;

// Per-instance data
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec2 aSize;
layout(location = 2) in vec2 aTexTopLeft;
layout(location = 3) in vec2 aTexBottomRight;
layout(location = 4) in vec4 aColor;
layout(location = 5) in float aItalics;
layout(location = 6) in float aRotation;

out vec4 vColor;
out vec2 vTexCoord;

// [0, 1]^2 to pixels
vec2 convertScreen01toPhysPixels(vec2 pos) {
    return pos * vec2(uPhysViewport.zw) + vec2(uPhysViewport.xy);
}

vec2 convertPhysPixelsToScreen01(vec2 pixels) {
    return (pixels - vec2(uPhysViewport.xy)) / vec2(uPhysViewport.zw);
}

vec2 anti_flicker(vec2 pos) {
    return convertPhysPixelsToScreen01(floor(convertScreen01toPhysPixels(pos)));
}

// [-1, 1]^2 to [0, 1]^2
vec2 convertNDCtoScreenSpace(vec2 pos) {
    return pos * 0.5 + 0.5;
}

// [0, 1]^2 to [-1, 1]^2
vec2 convertScreen01toNdcClip(vec2 pos) {
    return pos * 2.0 - 1.0;
}

const vec4 ignored = vec4(2.0, 2.0, 2.0, 1.0);

void main()
{
    // Pass color to fragment shader
    vColor = aColor;

    // Project world position to clip space
    vec4 pos = uMVP3D * aPos;

    // Cull glyphs that are way off-screen or behind the camera
    if (any(greaterThan(abs(pos.xyz), vec3(1.5 * abs(pos.w)))) || abs(pos.w) < 1e-3) {
        gl_Position = ignored;
        return;
    }

    // Convert to Normalized Device Coordinates (NDC)
    pos /= pos.w;

    // Convert to screen space [0,1], round to nearest pixel to prevent jitter, then convert back.
    vec2 wordOriginScreen = anti_flicker(convertNDCtoScreenSpace(pos.xy));

    // Determine corner vertex offset and texture coordinate from gl_VertexID
    vec2 cornerOffset;
    vec2 texCoord;

    if (gl_VertexID == 0) { // Top-left
        cornerOffset = vec2(0.0, 0.0);
        texCoord = aTexTopLeft;
    } else if (gl_VertexID == 1) { // Bottom-left
        cornerOffset = vec2(0.0, aSize.y);
        texCoord = vec2(aTexTopLeft.x, aTexBottomRight.y);
    } else if (gl_VertexID == 2) { // Top-right
        cornerOffset = vec2(aSize.x, 0.0);
        texCoord = vec2(aTexBottomRight.x, aTexTopLeft.y);
    } else { // Bottom-right
        cornerOffset = aSize;
        texCoord = aTexBottomRight;
    }
    vTexCoord = texCoord;

    // Apply italics shear transformation
    float yPosInQuad = cornerOffset.y;
    cornerOffset.x += aItalics * (aSize.y - yPosInQuad);

    // Apply rotation
    float angle = radians(aRotation);
    mat2 rotationMatrix = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    cornerOffset = rotationMatrix * (cornerOffset - aSize * 0.5) + aSize * 0.5;

    // Combine base position with the corner offset and convert back to NDC clip space
    pos.xy = convertScreen01toNdcClip(wordOriginScreen + convertPhysPixelsToScreen01(cornerOffset));

    gl_Position = pos;
}
