// See https://mattdesl.svbtle.com/drawing-lines-is-hard
// for a nice reference.

// The raw world-space vertex data.
layout(location = 0) in vec3 prev_pos_in;
layout(location = 1) in vec3 curr_pos_in;
layout(location = 2) in vec3 next_pos_in;
layout(location = 3) in vec4 color_in;
layout(location = 4) in float side_in;

// The combined model-view-projection matrix.
uniform mat4 mvp;

// The viewport size in pixels (e.g., [800, 600]).
uniform vec2 viewport_size;

// The desired line width in pixels.
uniform float line_width;

// The vertex color, passed to the fragment shader.
out vec4 color_out;
out float side_out;

// Helper to convert clip space coordinates to screen space.
vec2 clip_to_screen(vec4 clip)
{
    vec2 ndc = clip.xy / clip.w;
    return (ndc + 1.0) / 2.0 * viewport_size;
}

void main()
{
    color_out = color_in;
    side_out = side_in;

    // Project the world-space positions into clip space.
    vec4 prev_clip = mvp * vec4(prev_pos_in, 1.0);
    vec4 curr_clip = mvp * vec4(curr_pos_in, 1.0);
    vec4 next_clip = mvp * vec4(next_pos_in, 1.0);

    // Convert clip space to screen space for geometry calculations.
    vec2 prev_screen = clip_to_screen(prev_clip);
    vec2 curr_screen = clip_to_screen(curr_clip);
    vec2 next_screen = clip_to_screen(next_clip);

    // Calculate screen-space direction vectors.
    vec2 dir1 = curr_screen - prev_screen;
    vec2 dir2 = next_screen - curr_screen;

    // Determine the normal direction for the line segment.
    vec2 normal;
    if (prev_pos_in == curr_pos_in) {
        // Start of the line: use the direction of the first segment.
        normal = normalize(vec2(-dir2.y, dir2.x));
    } else if (next_pos_in == curr_pos_in) {
        // End of the line: use the direction of the last segment.
        normal = normalize(vec2(-dir1.y, dir1.x));
    } else {
        // Middle of the line (miter join): average the normals of the two segments.
        vec2 n1 = normalize(vec2(-dir1.y, dir1.x));
        vec2 n2 = normalize(vec2(-dir2.y, dir2.x));
        normal = normalize(n1 + n2);
    }

    // Calculate the pixel offset for the vertex.
    vec2 offset = normal * line_width * 0.5 * side_in;

    // Apply the offset in screen space and convert back to clip space.
    vec2 final_screen = curr_screen + offset;
    vec2 final_ndc = (final_screen / viewport_size) * 2.0 - 1.0;

    gl_Position = vec4(final_ndc * curr_clip.w, curr_clip.z, curr_clip.w);
}
