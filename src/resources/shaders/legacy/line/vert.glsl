// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

uniform mat4 uMVP;
uniform vec2 uViewport;

layout(location = 0) in vec3 aP1;
layout(location = 1) in vec3 aP2;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aThickness;
layout(location = 4) in vec2 aCorner;

out vec4 vColor;
out vec2 vLineCoord;

void main()
{
    vColor = aColor;

    // Output vertices to screen-space
    vec4 p1 = uMVP * vec4(aP1, 1.0);
    vec4 p2 = uMVP * vec4(aP2, 1.0);

    // Get screenspace line direction
    vec2 dir = normalize((p2.xy / p2.w) - (p1.xy / p1.w));

    // Get perpendicular direction and offset by thickness
    vec2 normal = vec2(-dir.y, dir.x);
    vec2 offset = (normal * aThickness / uViewport) * aCorner.x;

    // Extrude the line
    gl_Position = p1 + vec4(offset * p1.w, 0.0, 0.0);

    vLineCoord = aCorner;
}
