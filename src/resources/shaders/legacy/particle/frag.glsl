uniform sampler2D particle_texture;
uniform int particle_type; // 0 for rain, 1 for fog, 2 for snow, 3 for clouds
uniform float time;

in vec2 tex_coord;
out vec4 frag_color;

void main() {
    if (particle_type == 0) { // Rain
        frag_color = vec4(0.8, 0.8, 1.0, 0.5);
    } else if (particle_type == 3) { // Clouds
        vec2 uv = tex_coord + vec2(time * 0.01, 0.0);
        frag_color = texture(particle_texture, uv);
    } else { // Fog or Snow
        frag_color = texture(particle_texture, tex_coord);
    }
}
