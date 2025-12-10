#if IS_GLES
#version 300 es
#else
#version 330 core
#endif

#if IS_GLES
precision mediump float;
#endif

// Per-vertex inputs
layout (location = 0) in vec3 prev_vert_pos;
layout (location = 1) in vec3 curr_vert_pos;
layout (location = 2) in vec3 next_vert_pos;
layout (location = 3) in vec4 color;
layout (location = 4) in float side;

// Uniforms
uniform mat4 u_mvp;
uniform vec2 u_viewport_size;
uniform float u_line_width;

// Outputs to fragment shader
out vec4 v_color;
out float v_dist_ratio; // Goes from -1 to 1 across the line width

void main() {
    v_color = color;

    // Project vertices into clip space
    vec4 prev_clip = u_mvp * vec4(prev_vert_pos, 1.0);
    vec4 curr_clip = u_mvp * vec4(curr_vert_pos, 1.0);
    vec4 next_clip = u_mvp * vec4(next_vert_pos, 1.0);

    // Perspective divide to get NDC, then scale to screen space
    vec2 prev_screen = u_viewport_size * (prev_clip.xy / prev_clip.w);
    vec2 curr_screen = u_viewport_size * (curr_clip.xy / curr_clip.w);
    vec2 next_screen = u_viewport_size * (next_clip.xy / next_clip.w);

    // Determine the direction of the line segments in screen space
    vec2 dir1 = curr_screen - prev_screen;
    vec2 dir2 = next_screen - curr_screen;

    // The C++ side signals start/end points by duplicating vertices.
    bool is_start = prev_vert_pos == curr_vert_pos;
    bool is_end = next_vert_pos == curr_vert_pos;

    vec2 line_dir = is_start ? dir2 : dir1;
    if (length(line_dir) < 1e-6) line_dir = vec2(1.0, 0.0); // Fallback for zero-length lines

    // Perpendicular direction in screen space
    vec2 normal = normalize(vec2(-line_dir.y, line_dir.x));

    // Miter join logic
    if (!is_start && !is_end && length(dir1) > 1e-6 && length(dir2) > 1e-6) {
        vec2 n1 = normalize(vec2(-dir1.y, dir1.x));
        vec2 n2 = normalize(vec2(-dir2.y, dir2.x));
        normal = normalize(n1 + n2);
        // Cap miter length to prevent extreme spikes on sharp corners
        float miter_scale = 1.0 / max(0.2, dot(normal, n1));
        normal *= min(miter_scale, 5.0);
    }

    v_dist_ratio = side; // Pass -1 or 1 to fragment shader

    float half_width = u_line_width * 0.5;
    vec2 offset = normal * half_width * side;

    vec2 final_screen_pos = curr_screen + offset;

    // Convert back to NDC
    vec2 final_ndc = final_screen_pos / u_viewport_size;

    gl_Position = vec4(final_ndc, curr_clip.z / curr_clip.w, 1.0) * curr_clip.w;
}
