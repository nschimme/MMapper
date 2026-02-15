in ivec4 aVertTexCol; // xyz=pos, w=(colorId<<8 | tex_z)
uniform mat4 uMVP;
out vec2 vTexCoord;
flat out uint vColorId;
flat out uint vTexZ;

void main() {
    vec2 pos[4];
    pos[0] = vec2(0.0, 0.0);
    pos[1] = vec2(1.0, 0.0);
    pos[2] = vec2(1.0, 1.0);
    pos[3] = vec2(0.0, 1.0);

    vec2 p = pos[gl_VertexID % 4];
    vTexCoord = p;

    vec3 worldPos = vec3(float(aVertTexCol.x), float(aVertTexCol.y), float(aVertTexCol.z));
    worldPos.xy += p;

    gl_Position = uMVP * vec4(worldPos, 1.0);

    vColorId = uint(aVertTexCol.w >> 8);
    vTexZ = uint(aVertTexCol.w & 0xFF);
}
