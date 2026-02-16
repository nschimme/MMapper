// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aPos;
layout(location = 1) in float aHash;
layout(location = 2) in float aType;
layout(location = 3) in float aLife;

uniform float uDeltaTime;
uniform vec2 uPlayerPos;
uniform float uTime;

out vec2 vPos;
out float vHash;
out float vType;
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
    vHash = aHash;
    vType = aType;
    vec2 pos = aPos;
    float life = aLife;

    float speed;
    float decay;
    if (aType == 0.0) { // Rain
        speed = 20.0 + aHash * 5.0;
        decay = 0.5 + aHash * 0.5;
        pos.y -= uDeltaTime * speed;
    } else { // Snow
        speed = 2.0 + aHash * 1.0;
        decay = 0.2 + aHash * 0.3;
        pos.y -= uDeltaTime * speed;
        // Horizontal swaying
        pos.x += sin(uTime * 1.2 + aHash * 6.28) * 0.01;
    }

    life -= uDeltaTime * decay;

    // Spatial fading for snow: expire if in a "hole"
    if (aType == 1.0) {
        float hole = hash21(floor(pos.xy * 4.0));
        if (hole < 0.04) {
            life -= uDeltaTime * 10.0; // Expire quickly
        }
    }

    if (life <= 0.0) {
        life = 1.0;
        // Respawn at a random offset from player
        // We use a mix of uTime and aHash for randomness
        float r1 = rand(aHash + uTime);
        float r2 = rand(r1 + 1.234);
        pos = uPlayerPos + vec2(r1 * 40.0 - 20.0, r2 * 40.0 - 20.0);
    } else {
        // Toroidal wrap around player in a 40x40 box
        vec2 rel = pos - uPlayerPos;
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
