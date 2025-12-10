// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#if !defined(GL_ES) && (__VERSION__ < 150)
#error "Requires GLSL 150 or newer."
#endif

#ifdef GL_ES
precision highp float;
#endif

in vec4 v_color;
in vec2 v_line_coord; // (distance_along_line, distance_from_center)

out vec4 out_color;

void main()
{
    // Calculate the distance from the line's center, normalized to [0, 1] at the edge.
    float dist_from_center = abs(v_line_coord.y);

    // Use fwidth to determine the width of a pixel in the same coordinate space.
    // This allows for screen-resolution-independent anti-aliasing.
    float fw = fwidth(dist_from_center);

    // Use smoothstep to create a smooth transition from fully opaque to fully transparent
    // over the distance of one pixel at the line's edge.
    float alpha = 1.0 - smoothstep(1.0 - fw, 1.0, dist_from_center);

    // Combine the calculated alpha with the input vertex color.
    out_color = vec4(v_color.rgb, v_color.a * alpha);
}
