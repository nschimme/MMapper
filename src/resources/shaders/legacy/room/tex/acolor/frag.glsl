#include "../../../common/version.glsl"

precision highp float;

in vec2 vTexCoord;
flat in uint vColorId;
flat in uint vTexZ;

uniform sampler2DArray uTexture;

layout(std140) uniform NamedColorsBlock {
    vec4 uColors[256];
};

out vec4 fragColor;

void main() {
    vec4 color = texture(uTexture, vec3(vTexCoord, float(vTexZ)));
    vec4 tint = uColors[vColorId];
    fragColor = color * tint;
}
