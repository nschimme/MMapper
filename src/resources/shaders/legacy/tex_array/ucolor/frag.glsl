#version 150 core

in vec2 vTexCoord;
in float vTexLayer; // Received from vertex shader

uniform sampler2DArray tex_array_sampler; // New sampler type

out vec4 fragColor;

void main() {
    fragColor = texture(tex_array_sampler, vec3(vTexCoord, vTexLayer));
    // if (fragColor.a < 0.01) discard; // Optional alpha test
}
