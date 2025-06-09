
in vec2 v_tex_coord;
in float v_tex_layer;

uniform sampler2DArray u_tex_array_sampler;

out vec4 frag_color;

void main() {
    frag_color = texture(u_tex_array_sampler, vec3(v_tex_coord, v_tex_layer));
    if (frag_color.a < 0.01) { // Alpha test
        discard;
    }
}
