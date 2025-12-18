#version 330 core

in vec3 v_tex_coord;
in vec4 v_color;

uniform sampler2DArray u_texture;

out vec4 f_color;

void main()
{
    f_color = texture(u_texture, v_tex_coord) * v_color;
}
