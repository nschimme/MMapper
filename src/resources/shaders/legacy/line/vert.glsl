// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#if !defined(GL_ES) && (__VERSION__ < 150)
#error "Requires GLSL 150 or newer."
#endif

#ifdef GL_ES
precision highp float;
#endif

uniform mat4 u_mvp;
uniform vec2 u_viewport; // (width, height)

in vec3 in_p1; // First endpoint of the line segment
in vec3 in_p2; // Second endpoint of the line segment
in vec4 in_color; // Color of the line
in float in_width; // Width of the line in world units

out vec4 v_color;
out vec2 v_line_coord; // (distance_along_line, distance_from_center)

void main()
{
    v_color = in_color;

    // Project the endpoints to clip space
    vec4 p1_clip = u_mvp * vec4(in_p1, 1.0);
    vec4 p2_clip = u_mvp * vec4(in_p2, 1.0);

    // Convert to normalized device coordinates (NDC)
    vec2 p1_ndc = p1_clip.xy / p1_clip.w;
    vec2 p2_ndc = p2_clip.xy / p2_clip.w;

    // Convert to screen space
    vec2 p1_screen = (p1_ndc * 0.5 + 0.5) * u_viewport;
    vec2 p2_screen = (p2_ndc * 0.5 + 0.5) * u_viewport;

    // Calculate the screen-space direction and normal of the line
    vec2 line_dir = normalize(p2_screen - p1_screen);
    vec2 line_normal = vec2(-line_dir.y, line_dir.x);

    // Calculate the world-space line width in screen space pixels
    // This is an approximation, but it's good enough for our purposes.
    vec4 width_vec_clip = u_mvp * vec4(in_p1 + vec3(in_width, 0.0, 0.0), 1.0);
    float width_ndc = (width_vec_clip.x / width_vec_clip.w) - p1_ndc.x;
    float width_pixels = width_ndc * 0.5 * u_viewport.x;

    // Use gl_VertexID to determine which corner of the quad we're processing
    float side = float((gl_VertexID % 2) * 2 - 1); // -1 or 1
    float end_point = float((gl_VertexID / 2) * 2 - 1); // -1 or 1

    vec2 offset = line_normal * width_pixels * 0.5 * side;

    vec2 current_pos_screen = (end_point > 0.0) ? p2_screen : p1_screen;
    vec2 final_pos_screen = current_pos_screen + offset;

    // Convert back to NDC
    vec2 final_pos_ndc = (final_pos_screen / u_viewport) * 2.0 - 1.0;

    // Use the clip-space position of the closer endpoint to ensure correct depth
    vec4 base_clip = (end_point > 0.0) ? p2_clip : p1_clip;
    gl_Position = vec4(final_pos_ndc * base_clip.w, base_clip.z, base_clip.w);

    v_line_coord = vec2(end_point * 0.5 + 0.5, side); // (0..1, -1..1)
}
