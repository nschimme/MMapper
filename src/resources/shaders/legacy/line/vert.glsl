// The raw world-space vertex data.
layout(location = 0) in vec3 prev_pos_in;
layout(location = 1) in vec3 curr_pos_in;
layout(location = 2) in vec3 next_pos_in;
layout(location = 3) in vec4 color_in;
layout(location = 4) in float side_in;

// The combined model-view-projection matrix.
uniform mat4 mvp;

// The camera's position in world-space.
uniform vec3 camera_pos_ws;

// The desired line width in world-space units.
uniform float line_width;

// The vertex color, passed to the fragment shader.
out vec4 color_out;
out float side_out;

const float EPSILON = 0.0001;

void main()
{
    color_out = color_in;
    side_out = side_in;

    vec2 dir1 = curr_pos_in.xy - prev_pos_in.xy;
    vec2 dir2 = next_pos_in.xy - curr_pos_in.xy;

    bool is_prev_endpoint = (prev_pos_in == curr_pos_in);
    bool is_next_endpoint = (next_pos_in == curr_pos_in);

    // A segment is "vertical" if its projection onto the XY plane is basically a point.
    bool is_incoming_vertical = !is_prev_endpoint && (length(dir1) < EPSILON);
    bool is_outgoing_vertical = !is_next_endpoint && (length(dir2) < EPSILON);

    vec2 normal;
    if (is_incoming_vertical || is_outgoing_vertical) {
        // For vertical segments or corners connected to them, billboard around the Z-axis.
        // The normal is perpendicular to the view direction in the XY plane.
        vec2 view_dir = normalize(curr_pos_in.xy - camera_pos_ws.xy);
        normal = vec2(-view_dir.y, view_dir.x);
    } else {
        // For horizontal segments, calculate the miter join in the XY plane.
        if (is_prev_endpoint) {
            // Start of the line
            normal = normalize(vec2(-dir2.y, dir2.x));
        } else if (is_next_endpoint) {
            // End of the line
            normal = normalize(vec2(-dir1.y, dir1.x));
        } else {
            // Middle of the line (miter join)
            vec2 n1 = normalize(vec2(-dir1.y, dir1.x));
            vec2 n2 = normalize(vec2(-dir2.y, dir2.x));
            normal = normalize(n1 + n2);
        }
    }

    // Calculate the offset in world-space.
    vec2 offset = normal * line_width * 0.5;

    // Apply the offset to the current vertex's XY position.
    vec3 final_pos = curr_pos_in;
    final_pos.xy += offset * side_in;

    // Transform the final position into clip space.
    gl_Position = mvp * vec4(final_pos, 1.0);
}
