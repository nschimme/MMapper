#version 330 core

// Inputs from vertex shader
in vec3 v_tex_coord;
in vec4 v_color;

// Uniforms
uniform sampler2DArray u_texture;

// Output color
out vec4 f_color;

void main()
{
    // Sample the texture and apply the color
    f_color = texture(u_texture, v_tex_coord) * v_color;
}
