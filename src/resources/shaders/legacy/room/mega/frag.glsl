precision highp float;
precision highp int;

in vec2 vTexCoord;
in float vLayerDepth;
flat in uint vTerrainIdx;
flat in uint vTrailIdx;
flat in uint vFlags;
flat in uint vOverlays1;
flat in uint vOverlays2;
flat in uint vWallInfo[3];
flat in uint vHighlight;

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

uniform int uWallLayers[4];
uniform int uDottedWallLayers[4];
uniform int uDoorLayers[6];
uniform int uStreamInLayers[6];
uniform int uStreamOutLayers[6];
uniform int uExitLayers[4];

out vec4 fragColor;

vec4 getWallColor(uint info) {
    uint colorIdx = (info >> 3) & 0xFFu;
    return uColors[colorIdx];
}

void main() {
    // Note: discard check moved to vertex shader for performance,
    // but kept here as safety for pixels.

    vec4 color = vec4(0.0);

    // Layer color logic
    vec4 layerColor = vec4(1.0, 1.0, 1.0, 0.90); // Focused or below
    if (vLayerDepth > 0.0) {
        layerColor = vec4(0.70, 0.70, 0.70, 0.20); // Gray70, alpha 0.2
    }

    // Terrain
    color = texture(uTerrainRoadArray, vec3(vTexCoord, float(vTerrainIdx)));

    // Time of day / Light tint
    vec4 tint = uTimeOfDayColor;
    if ((vFlags & 1u) != 0u) {
        // Dark room tint (NamedColorEnum::ROOM_DARK = 12)
        tint *= uColors[12];
    } else if ((vFlags & 2u) != 0u) {
        // No sundeath tint (NamedColorEnum::ROOM_NO_SUNDEATH = 13)
        tint *= uColors[13];
    }
    color *= tint * layerColor;

    // Trail
    if (vTrailIdx != 0xFFFFu) {
        vec4 trailColor = texture(uTrailArray, vec3(vTexCoord, float(vTrailIdx)));
        color = mix(color, trailColor, trailColor.a);
    }

    // Overlays (Packed indices)
    uint o[8];
    o[0] = vOverlays1 & 0xFFu;
    o[1] = (vOverlays1 >> 8) & 0xFFu;
    o[2] = (vOverlays1 >> 16) & 0xFFu;
    o[3] = (vOverlays1 >> 24) & 0xFFu;
    o[4] = vOverlays2 & 0xFFu;
    o[5] = (vOverlays2 >> 8) & 0xFFu;
    o[6] = (vOverlays2 >> 16) & 0xFFu;
    o[7] = (vOverlays2 >> 24) & 0xFFu;

    for (int i = 0; i < 8; ++i) {
        if (o[i] == 0xFFu) continue;
        vec4 overlayColor = texture(uOverlayArray, vec3(vTexCoord, float(o[i])));
        color = mix(color, overlayColor, overlayColor.a);
    }

    // Walls & Doors (NESW only: 0-3)
    for (int i = 0; i < 4; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        uint type = info & 0x7u;
        if (type == 0u) continue;

        vec4 wallTexColor = vec4(0.0);
        // type 1=SOLID, 2=DOTTED, 3=DOOR
        if (type == 1u)
            wallTexColor = texture(uWallArray, vec3(vTexCoord, float(uWallLayers[i])));
        else if (type == 2u)
            wallTexColor = texture(uDottedWallArray, vec3(vTexCoord, float(uDottedWallLayers[i])));
        else if (type == 3u)
            wallTexColor = texture(uDoorArray, vec3(vTexCoord, float(uDoorLayers[i])));

        vec4 wallColor = getWallColor(info);
        color = mix(color, wallTexColor * wallColor, wallTexColor.a * wallColor.a);
    }

    // Stream icons (Flow)
    // NamedColorEnum::STREAM = 14
    vec4 streamTint = uColors[14] * layerColor;
    for (int i = 0; i < 6; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        if ((info & 0x800u) != 0u) { // OutFlow
            vec4 streamOut = texture(uStreamOutArray, vec3(vTexCoord, float(uStreamOutLayers[i])));
            color = mix(color, streamOut * streamTint, streamOut.a * streamTint.a);
        }
        if ((info & 0x4000u) != 0u) { // InFlow
            vec4 streamIn = texture(uStreamInArray, vec3(vTexCoord, float(uStreamInLayers[i])));
            color = mix(color, streamIn * streamTint, streamIn.a * streamTint.a);
        }
    }

    // Vertical exit icons (Up/Down)
    for (int i = 4; i < 6; ++i) {
        uint info = (vWallInfo[i / 2] >> (16 * (i % 2))) & 0xFFFFu;
        bool isExit = (info & 0x2000u) != 0u;
        bool isClimb = (info & 0x1000u) != 0u;
        if (isExit || isClimb) {
            // uExitLayers: climb-down, climb-up, down, up
            int iconIdx = (i == 4) ? (isClimb ? 1 : 3) : (isClimb ? 0 : 2);
            vec4 iconColor = texture(uExitIconArray, vec3(vTexCoord, float(uExitLayers[iconIdx])));
            color = mix(color, iconColor * layerColor, iconColor.a * layerColor.a);
        }
    }

    // Layer-based depth overlay (darker/lighter)
    if (vLayerDepth != 0.0) {
        float baseAlpha = (vLayerDepth < 0.0) ? 0.5 : 0.1;
        float alpha = clamp(baseAlpha + 0.03 * abs(vLayerDepth), 0.0, 1.0);
        vec4 overlay;
        if (vLayerDepth < 0.0 || !uDrawUpperLayersTextured) {
            overlay = vec4(0.0, 0.0, 0.0, alpha); // Black
        } else {
            overlay = vec4(1.0, 1.0, 1.0, alpha); // White
        }
        color = mix(color, overlay, overlay.a);
    }

    // Highlights (Diff)
    if (vHighlight != 0u) {
        vec4 hColor = uColors[vHighlight];
        color = mix(color, hColor, hColor.a);
    }

    fragColor = color;
}
