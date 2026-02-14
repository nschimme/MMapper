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

    // Overlays (Loads)
    for (int i = 0; i < 25; ++i) {
        if ((vLoadFlags & (1u << uint(i))) != 0u) {
            vec4 overlayColor = texture(uOverlayArray, vec3(vTexCoord, float(i)));
            color = mix(color, overlayColor, overlayColor.a);
        }
    }

    // Overlays (Mobs)
    for (int i = 0; i < 19; ++i) {
        if ((vMobFlags & (1u << uint(i))) != 0u) {
            vec4 overlayColor = texture(uOverlayArray, vec3(vTexCoord, float(25 + i)));
            color = mix(color, overlayColor, overlayColor.a);
        }
    }

    // No Ride
    if ((vFlags & 4u) != 0u) {
        vec4 overlayColor = texture(uOverlayArray, vec3(vTexCoord, float(25 + 19)));
        color = mix(color, overlayColor, overlayColor.a);
    }

    // Walls & Doors
    for (int i = 0; i < 6; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        uint type = info & 0xFu;
        if (type == 0u) continue;

        vec4 wallTexColor = vec4(0.0);
        // type 1=SOLID, 2=DOTTED, 3=DOOR
        if (type == 1u)
            wallTexColor = texture(uWallArray, vec3(vTexCoord, float(i < 4 ? i : (i - 4))));
        else if (type == 2u)
            wallTexColor = texture(uDottedWallArray, vec3(vTexCoord, float(i < 4 ? i : (i - 4))));
        else if (type == 3u)
            wallTexColor = texture(uDoorArray, vec3(vTexCoord, float(i)));

        vec4 wallColor = getWallColor(info);
        color = mix(color, wallTexColor * wallColor, wallTexColor.a * wallColor.a);
    }

    // Stream icons (Flow)
    for (int i = 0; i < 6; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        if ((info & 0x1000u) != 0u) {
            vec4 streamIn = texture(uStreamInArray, vec3(vTexCoord, float(i)));
            vec4 streamOut = texture(uStreamOutArray, vec3(vTexCoord, float(i)));
            color = mix(color, streamIn, streamIn.a);
            color = mix(color, streamOut, streamOut.a);
        }
    }

    // Vertical exit icons (Up/Down)
    for (int i = 4; i < 6; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        bool isExit = (info & 0x4000u) != 0u;
        bool isClimb = (info & 0x2000u) != 0u;
        if (isExit || isClimb) {
            uint iconIdx = (i == 4) ? (isClimb ? 1u : 3u) : (isClimb ? 0u : 2u);
            vec4 iconColor = texture(uExitIconArray, vec3(vTexCoord, float(iconIdx)));
            color = mix(color, iconColor, iconColor.a);
        }
    }

    fragColor = color * fade;
}
