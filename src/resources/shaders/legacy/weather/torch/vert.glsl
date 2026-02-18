// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos;        // xyz, w=zScale
    vec4 uIntensities;      // precip_start, clouds_start, fog_start, type_start
    vec4 uTargets;          // precip_target, clouds_target, fog_target, type_target
    vec4 uTimeOfDayIndices; // x=startIdx, y=targetIdx, z=timeOfDayIntensityStart, w=timeOfDayIntensityTarget
    vec4 uConfig;           // x=weatherStartTime, y=timeOfDayStartTime, z=duration, w=unused
    vec4 uTorch;            // x=startIntensity, y=targetIntensity, z=colorIdx, w=isRoomDark
};

out vec3 vWorldPos;

void main()
{
    // Quad vertices 0..3 for TRIANGLE_STRIP
    // 0: (-1, -1), 1: (1, -1), 2: (-1, 1), 3: (1, 1)
    vec2 offset = vec2(float(gl_VertexID & 1) * 2.0 - 1.0,
                       float((gl_VertexID >> 1) & 1) * 2.0 - 1.0);

    // Cover a 3.5-unit radius (7x7 units area)
    vec2 worldXY = uPlayerPos.xy + offset * 4.0;
    vWorldPos = vec3(worldXY, uPlayerPos.z);

    // Project to screen space
    gl_Position = uViewProj * vec4(vWorldPos.x, vWorldPos.y, vWorldPos.z * uPlayerPos.w, 1.0);
}
