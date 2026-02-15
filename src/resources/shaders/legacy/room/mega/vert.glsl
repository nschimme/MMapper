#include "../../common/version.glsl"

in ivec3 aPos;
in uint aTerrainTrail;
in uint aFlags;
in uint aMobFlags;
in uint aLoadFlags;
in uint aWallInfo0;
in uint aWallInfo1;
in uint aWallInfo2;
in uint aHighlight;

uniform mat4 uViewProj;
uniform int uCurrentLayer;

out vec2 vTexCoord;
out float vLayerDepth;
flat out uint vTerrainIdx;
flat out uint vTrailIdx;
flat out uint vFlags;
flat out uint vMobFlags;
flat out uint vLoadFlags;
flat out uint vWallInfo[3];
flat out uint vHighlight;

void main() {
    // Quad vertices: 0, 1, 2, 3 (Triangle Fan)
    vec2 pos[4];
    pos[0] = vec2(-0.5, -0.5);
    pos[1] = vec2( 0.5, -0.5);
    pos[2] = vec2( 0.5,  0.5);
    pos[3] = vec2(-0.5,  0.5);

    vec2 p = pos[gl_VertexID % 4];
    vTexCoord = p + 0.5;

    vec3 worldPos = vec3(float(aPos.x), float(aPos.y), float(aPos.z));
    worldPos.xy += p;

    gl_Position = uViewProj * vec4(worldPos, 1.0);

    vLayerDepth = float(aPos.z - uCurrentLayer);
    vTerrainIdx = aTerrainTrail & 0xFFFFu;
    vTrailIdx = aTerrainTrail >> 16;
    vFlags = aFlags;
    vMobFlags = aMobFlags;
    vLoadFlags = aLoadFlags;
    vWallInfo[0] = aWallInfo0;
    vWallInfo[1] = aWallInfo1;
    vWallInfo[2] = aWallInfo2;
    vHighlight = aHighlight;
}
