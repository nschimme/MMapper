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
    vec4 uTorch;            // x=startIntensity, y=targetIntensity, z=colorIdx, w=unused
};

out vec3 vWorldPos;

void main()
{
    // Full screen triangle
    vec4 pos = vec4(vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0, 0.0, 1.0);
    gl_Position = pos;

    // Unproject to world space at z=0 (approximately)
    mat4 invVP = inverse(uViewProj);
    vec4 worldPos = invVP * pos;
    vWorldPos = worldPos.xyz / worldPos.w;
}
