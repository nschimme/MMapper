precision highp float;
precision highp int;

in ivec3 aPos;
in uint aTerrainTrail;
in uint aFlags;
in uint aOverlays1;
in uint aOverlays2;
in uint aWallInfo0;
in uint aWallInfo1;
in uint aWallInfo2;
in uint aHighlight;

uniform mat4 uViewProj;
uniform int uCurrentLayer;
uniform int uMinZ;
uniform int uMaxZ;
uniform vec2 uMinBounds;
uniform vec2 uMaxBounds;

out vec2 vTexCoord;
out float vLayerDepth;
flat out uint vTerrainIdx;
flat out uint vTrailIdx;
flat out uint vFlags;
flat out uint vOverlays1;
flat out uint vOverlays2;
flat out uint vWallInfo[3];
flat out uint vHighlight;

void main() {
    // Cull rooms outside the visible bounds or outside the visible Z range in the vertex shader
    // Setting position to a point outside the clip volume (-1..1 range)
    if (aPos.z < uMinZ || aPos.z > uMaxZ ||
        float(aPos.x) < uMinBounds.x - 1.0 || float(aPos.x) > uMaxBounds.x + 1.0 ||
        float(aPos.y) < uMinBounds.y - 1.0 || float(aPos.y) > uMaxBounds.y + 1.0 ||
        (aFlags & 8u) == 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    // Quad vertices: 0, 1, 2, 3 (Triangle Fan) - Range 0..1
    vec2 pos[4];
    pos[0] = vec2(0.0, 0.0);
    pos[1] = vec2(1.0, 0.0);
    pos[2] = vec2(1.0, 1.0);
    pos[3] = vec2(0.0, 1.0);

    vec2 p = pos[gl_VertexID % 4];
    vTexCoord = p;

    vec3 worldPos = vec3(float(aPos.x), float(aPos.y), float(aPos.z));
    worldPos.xy += p;

    gl_Position = uViewProj * vec4(worldPos, 1.0);

    vLayerDepth = float(aPos.z - uCurrentLayer);
    vTerrainIdx = aTerrainTrail & 0xFFFFu;
    vTrailIdx = aTerrainTrail >> 16;
    vFlags = aFlags;
    vOverlays1 = aOverlays1;
    vOverlays2 = aOverlays2;
    vWallInfo[0] = aWallInfo0;
    vWallInfo[1] = aWallInfo1;
    vWallInfo[2] = aWallInfo2;
    vHighlight = aHighlight;
}
