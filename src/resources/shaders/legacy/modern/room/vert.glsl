#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in vec4 a_color;

uniform mat4 u_projection;

out vec3 v_tex_coord;
out vec4 v_color;

void main()
{
    vec2 quad_vertices[4] = vec2[](
        vec2(0, 0),
        vec2(1, 0),
        vec2(1, 1),
        vec2(0, 1)
    );

    vec2 vertex = quad_vertices[gl_VertexID];
    gl_Position = u_projection * vec4(a_position.xy + vertex, a_position.z, 1.0);
    v_tex_coord = vec3(vertex, a_tex_coord.z);
    v_color = a_color;
}
