uniform mat4 mvp_matrix;

in vec3 position;
in vec2 tex_coord_in;

out vec2 tex_coord;

void main() {
    gl_Position = mvp_matrix * vec4(position, 1.0);
    tex_coord = tex_coord_in;
}
