#include "../../common/version.glsl"

precision highp float;
precision highp int;

in vec2 vTexCoord;
in float vLayerDepth;
flat in uint vTerrainIdx;
flat in uint vTrailIdx;
flat in uint vFlags;
flat in uint vMobFlags;
flat in uint vLoadFlags;
flat in uint vWallInfo[3];

uniform sampler2DArray uTerrainRoadArray;
uniform sampler2DArray uTrailArray;
uniform sampler2DArray uOverlayArray;
uniform sampler2DArray uWallArray;
uniform sampler2DArray uDottedWallArray;
uniform sampler2DArray uDoorArray;
uniform sampler2DArray uStreamInArray;
uniform sampler2DArray uStreamOutArray;
uniform sampler2DArray uExitIconArray;

uniform bool uDrawUpperLayersTextured;
uniform vec4 uTimeOfDayColor;

layout(std140) uniform NamedColorsBlock {
    vec4 uColors[256];
};

out vec4 fragColor;

vec4 getWallColor(uint info) {
    uint colorIdx = (info >> 4) & 0xFFu;
    return uColors[colorIdx];
}

void main() {
    if ((vFlags & 8u) == 0u) discard;

    float absDepth = abs(vLayerDepth);
    if (absDepth > 1.0 && !uDrawUpperLayersTextured) discard;

    vec4 color = vec4(0.0);

    // Fading for layer (simplified without texture for now)
    float fade = 1.0;
    if (vLayerDepth > 0.0) fade = 0.5;
    else if (vLayerDepth < 0.0) fade = 0.25;

    // Terrain
    color = texture(uTerrainRoadArray, vec3(vTexCoord, float(vTerrainIdx)));

    // Time of day / Light tint
    vec4 tint = uTimeOfDayColor;
    if ((vFlags & 1u) != 0u) {
        // Dark room tint
        tint *= vec4(0.5, 0.5, 0.8, 1.0);
    }
    color *= tint;

    // Trail
    if (vTrailIdx != 0xFFFFu) {
        vec4 trailColor = texture(uTrailArray, vec3(vTexCoord, float(vTrailIdx)));
        color = mix(color, trailColor, trailColor.a);
    }

    // Overlays (Mobs, Loads)
    if (vMobFlags != 0u) {
        // TODO: Map mob flags to overlay array index if needed
        // For now assume mob overlay is at some index, or use the uOverlayArray
        // In Textures.cpp, mob and load are combined into one array.
    }

    // Walls & Doors
    for (int i=0; i<6; ++i) {
        uint info = (vWallInfo[i/2] >> (16 * (i%2))) & 0xFFFFu;
        uint type = info & 0xFu;
        if (type == 0u) continue;

        vec4 wallTexColor = vec4(0.0);
        // type 1=SOLID, 2=DOTTED, 3=DOOR
        if (type == 1u) wallTexColor = texture(uWallArray, vec3(vTexCoord, float(i < 4 ? i : (i - 4))));
        else if (type == 2u) wallTexColor = texture(uDottedWallArray, vec3(vTexCoord, float(i < 4 ? i : (i - 4))));
        else if (type == 3u) wallTexColor = texture(uDoorArray, vec3(vTexCoord, float(i)));

        vec4 wallColor = getWallColor(info);
        color = mix(color, wallTexColor * wallColor, wallTexColor.a * wallColor.a);
    }

    fragColor = color * fade;
}
