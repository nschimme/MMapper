// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aPos;
layout(location = 1) in float aLife;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    mat4 uInvViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uWeatherIntensities; // x=rain, y=snow, z=clouds, w=fog
    vec4 uTimeOfDayColor;
    vec4 uTimeAndDelta; // x=time, y=deltaTime
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
    float type = (gl_VertexID < 4096) ? 0.0 : 1.0;

    vec2 pos = aPos;
    float life = aLife;

    float speed;
    float decay;
    float uTime = uTimeAndDelta.x;
    float uDeltaTime = uTimeAndDelta.y;

    if (type == 0.0) { // Rain
        speed = 20.0 + hash * 5.0;
        decay = 0.5 + hash * 0.5;
        pos.y -= uDeltaTime * speed;
    } else { // Snow
        speed = 2.0 + hash * 1.0;
        decay = 0.2 + hash * 0.3;
        pos.y -= uDeltaTime * speed;
        // Horizontal swaying
        pos.x += sin(uTime * 1.2 + hash * 6.28) * 0.01;
    }

    life -= uDeltaTime * decay;

    // Spatial fading for snow: expire if in a "hole"
    if (type == 1.0) {
        float hole = hash21(floor(pos.xy * 4.0));
        if (hole < 0.04) {
            life -= uDeltaTime * 10.0; // Expire quickly
        }
    }

    if (life <= 0.0) {
        life = 1.0;
        // Respawn at a random offset from player
        // We use a mix of uTime and aHash for randomness
        float r1 = rand(hash + uTime);
        float r2 = rand(r1 + 1.234);
        pos = uPlayerPos.xy + vec2(r1 * 40.0 - 20.0, r2 * 40.0 - 20.0);
    } else {
        // Toroidal wrap around player in a 40x40 box
        vec2 rel = pos - uPlayerPos.xy;
        if (rel.x < -20.0)
            pos.x += 40.0;
        else if (rel.x > 20.0)
            pos.x -= 40.0;

        if (rel.y < -20.0)
            pos.y += 40.0;
        else if (rel.y > 20.0)
            pos.y -= 40.0;
    }

    vPos = pos;
    vLife = life;
    gl_Position = vec4(vPos, 0.0, 1.0);
}
