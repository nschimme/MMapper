#version 150 core

in vec3 aPosition;
in vec2 aTexCoord;
in float aTexLayer; // New attribute for texture layer

uniform mat4 uModelViewProjectionMatrix;

out vec2 vTexCoord;
out float vTexLayer;

void main() {
    gl_Position = uModelViewProjectionMatrix * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    vTexLayer = aTexLayer;
}
