// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

uniform mat4 uMVP;

// Per-vertex attribute for the base quad (4 vertices per instance)
// (0, -0.5), (1, -0.5), (0, 0.5), (1, 0.5) for a unit line quad
layout(location = 0) in vec2 baseVertexPosition;

// Per-instance attributes (data changes per line segment)
layout(location = 1) in vec2 aInstanceStartPoint;
layout(location = 2) in vec2 aInstanceEndPoint;
layout(location = 3) in float aInstanceThickness;
layout(location = 4) in vec4 aInstanceColor;

out vec4 vColor; // Pass instance color to fragment shader

void main() {
    vec2 segmentVector = aInstanceEndPoint - aInstanceStartPoint;
    float segmentLength = length(segmentVector);

    // Handle zero-length segments to avoid division by zero or NaN issues
    if (segmentLength == 0.0) {
        // Collapse all vertices of the quad to the start point.
        // This makes the quad degenerate but avoids visual artifacts or errors.
        gl_Position = uMVP * vec4(aInstanceStartPoint, 0.0, 1.0);
        vColor = aInstanceColor; // Still pass color
        return;
    }

    vec2 segmentDirection = segmentVector / segmentLength; // Normalized direction
    vec2 segmentNormal = vec2(-segmentDirection.y, segmentDirection.x); // Perpendicular vector

    // Scale the base vertex position:
    // baseVertexPosition.x is [0, 1] along the segment's direction
    // baseVertexPosition.y is [-0.5, 0.5] along the segment's normal
    vec2 scaledPosition = vec2(baseVertexPosition.x * segmentLength,
                               baseVertexPosition.y * aInstanceThickness);

    // Calculate final position by starting at instance_startPoint and adding
    // scaled components along segmentDirection and segmentNormal.
    vec2 finalPosition = aInstanceStartPoint +
                         (segmentDirection * scaledPosition.x) +
                         (segmentNormal * scaledPosition.y);

    gl_Position = uMVP * vec4(finalPosition, 0.0, 1.0);
    vColor = aInstanceColor;
}
