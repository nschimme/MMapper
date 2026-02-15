// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform float uDeltaTime;
uniform float uTime;
uniform vec3 uPlayerPos;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inVel;
layout(location = 2) in float inLife;
layout(location = 3) in float inType;

out vec3 outPos;
out vec3 outVel;
out float outLife;
out float outType;

float hash11(float p)
{
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main()
{
    // Simulation Box (Toroidal Volume)
    vec3 boxHalfSize = vec3(20.0, 20.0, 30.0);
    vec3 boxSize = boxHalfSize * 2.0;

    vec3 pos = inPos;
    vec3 vel = inVel;
    float life = inLife;
    float type = inType;

    if (life <= 0.0) {
        // Initialize based on ID and time to create a stable grid/distribution
        float h = hash11(float(gl_VertexID) * 0.123 + uTime * 0.0001);
        type = step(0.5, hash11(h + 0.4));

        pos.x = (hash11(h + 0.1) - 0.5) * boxSize.x;
        pos.y = (hash11(h + 0.2) - 0.5) * boxSize.y;
        pos.z = (hash11(h + 0.3) - 0.5) * boxSize.z;
        pos += uPlayerPos;

        if (type < 0.5) { // Rain
             float col = floor(pos.x * 12.0);
             float h_col = hash11(col);
             float speed = 20.0 + h_col * 5.0;
             vel = vec3(0.0, 0.0, -speed);
        } else { // Snow
             vel = vec3(0.0, 0.0, -2.0);
        }
        life = 10.0;
    }

    // Physics
    pos += vel * uDeltaTime;

    // Toroidal Wrap
    vec3 relPos = pos - uPlayerPos;
    if (relPos.x < -boxHalfSize.x) pos.x += boxSize.x;
    if (relPos.x >  boxHalfSize.x) pos.x -= boxSize.x;
    if (relPos.y < -boxHalfSize.y) pos.y += boxSize.y;
    if (relPos.y >  boxHalfSize.y) pos.y -= boxSize.y;
    if (relPos.z < -boxHalfSize.z) pos.z += boxSize.z;
    if (relPos.z >  boxHalfSize.z) pos.z -= boxSize.z;

    outPos = pos;
    outVel = vel;
    outLife = life;
    outType = type;
}
