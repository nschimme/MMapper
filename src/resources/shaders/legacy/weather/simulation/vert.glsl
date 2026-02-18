// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aPos;
layout(location = 1) in float aLife;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos;        // xyz, w=zScale
    vec4 uIntensities;      // precip_start, clouds_start, fog_start, type_start
    vec4 uTargets;          // precip_target, clouds_target, fog_target, type_target
    vec4 uTimeOfDayIndices; // x=startIdx, y=targetIdx, z=todIntStart, w=todIntTarget
    vec4 uConfig;           // x=weatherStartTime, y=timeOfDayStartTime, z=duration, w=unused
};

layout(std140) uniform TimeBlock
{
    vec2 uTime; // x=time, y=delta
};

out vec2 vPos;
out float vLife;

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float rand(float n)
{
    return fract(sin(n) * 43758.5453123);
}

void main()
{
    float hash = rand(float(gl_VertexID));
    float uCurrentTime = uTime.x;
    float uDeltaTime = uTime.y;
    float uWeatherStartTime = uConfig.x;
    float uTransitionDuration = uConfig.z;

    float weatherLerp = clamp((uCurrentTime - uWeatherStartTime) / uTransitionDuration, 0.0, 1.0);
    float pIntensity = mix(uIntensities.x, uTargets.x, weatherLerp);
    float pType = mix(uIntensities.w, uTargets.w, weatherLerp); // 0=rain, 1=snow

    vec2 pos = aPos;
    float life = aLife;

    // Rain physics
    float rainSpeed = (15.0 + pIntensity * 10.0) + hash * 5.0;
    float rainDecay = 0.35 + hash * 0.15;

    // Snow physics
    float snowSpeed = (0.75 + pIntensity * 0.75) + hash * 0.5;
    float snowDecay = 0.015 + hash * 0.01;

    // Interpolate physics based on pType
    float speed = mix(rainSpeed, snowSpeed, pType);
    float decay = mix(rainDecay, snowDecay, pType);

    pos.y -= uDeltaTime * speed;

    // Horizontal swaying (only for snow)
    pos.x += sin(uCurrentTime * 1.2 + hash * 6.28) * (0.005 + pIntensity * 0.01) * pType;

    life -= uDeltaTime * decay;

    // Spatial fading
    float hole = hash21(floor(pos.xy * 4.0));
    if (hole < 0.02) {
        life -= uDeltaTime * 2.0; // Expire quickly but not instantly
    }

    if (life <= 0.0) {
        life = 1.0;
        // Respawn at the top
        float r1 = rand(hash + uCurrentTime);
        pos.x = uPlayerPos.x + (r1 * 28.0 - 14.0);
        pos.y = uPlayerPos.y + 14.0;
    } else {
        // Toroidal wrap around player (28.0x28.0 area)
        vec2 rel = pos - uPlayerPos.xy;
        if (rel.x < -14.0)
            pos.x += 28.0;
        else if (rel.x > 14.0)
            pos.x -= 28.0;

        if (rel.y < -14.0)
            pos.y += 28.0;
        else if (rel.y > 14.0)
            pos.y -= 28.0;
    }

    vPos = pos;
    vLife = life;
    gl_Position = vec4(vPos, 0.0, 1.0);
}
