#version 330 core

// Vertex attributes (per-instance)
in vec3 a_position;
in vec3 a_tex_coord;
in vec4 a_color;

// Uniforms
uniform mat4 u_projection;

// Outputs to fragment shader
out vec3 v_tex_coord;
out vec4 v_color;

void main()
{
    // Generate a quad on the fly
    vec2 quad_verts[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    // Calculate the vertex position
    vec2 vertex_pos = a_position.xy + quad_verts[gl_VertexID];
    gl_Position = u_projection * vec4(vertex_pos, a_position.z, 1.0);

    // Pass texture coordinates and color to the fragment shader
    v_tex_coord = a_tex_coord;
    v_color = a_color;
}
