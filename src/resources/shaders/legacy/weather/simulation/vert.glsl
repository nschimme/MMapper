// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aPos;
layout(location = 1) in float aLife;

layout(std140) uniform WeatherBlock
{
    mat4 uViewProj;
    vec4 uPlayerPos; // xyz, w=zScale
    vec4 uIntensitiesStart;
    vec4 uIntensitiesTarget;
    vec4 uToDColorStart;
    vec4 uToDColorTarget;
    vec4 uTimes; // x=weatherStart, y=todStart, z=time, w=delta
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

float get_intensity(int idx)
{
    float uTime = uTimes.z;
    float t = clamp((uTime - uTimes.x) / 2.0, 0.0, 1.0);
    float s = smoothstep(0.0, 1.0, t);
    return mix(uIntensitiesStart[idx], uIntensitiesTarget[idx], s);
}

void main()
{
    float hash = rand(float(gl_VertexID));
    float type = (gl_VertexID < 4096) ? 0.0 : 1.0;

    vec2 pos = aPos;
    float life = aLife;

    float speed;
    float decay;
    float uTime = uTimes.z;
    float uDeltaTime = uTimes.w;

    if (type == 0.0) { // Rain
        float rainIntensity = get_intensity(0);
        // Speed increases with intensity: 15.0 at low, 25.0 at heavy (1.0), 35.0 at boosted (2.0)
        speed = (15.0 + rainIntensity * 10.0) + hash * 5.0;
        decay = 0.5 + hash * 0.5;
        pos.y -= uDeltaTime * speed;
    } else { // Snow
        float snowIntensity = get_intensity(1);
        // Snow speed: 0.75 at low, 1.5 at heavy (1.0)
        speed = (0.75 + snowIntensity * 0.75) + hash * 0.5;
        decay = 0.2 + hash * 0.3;
        pos.y -= uDeltaTime * speed;
        // Horizontal swaying increases with intensity
        pos.x += sin(uTime * 1.2 + hash * 6.28) * (0.005 + snowIntensity * 0.01);
    }

    life -= uDeltaTime * decay;

    // Spatial fading: expire if in a "hole"
    float hole = hash21(floor(pos.xy * 4.0));
    if (hole < 0.04) {
        life -= uDeltaTime * 10.0; // Expire quickly
    }

    if (life <= 0.0) {
        life = 1.0;
        // Respawn at the top of the simulation volume (40x40 box around player)
        // We randomize X position but keep Y at the top (+20.0)
        float r1 = rand(hash + uTime);
        pos.x = uPlayerPos.x + (r1 * 40.0 - 20.0);
        pos.y = uPlayerPos.y + 20.0;
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
