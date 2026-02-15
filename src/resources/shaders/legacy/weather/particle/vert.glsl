// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uViewProj;
uniform vec3 uPlayerPos;
uniform float uTime;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inVel;
layout(location = 2) in float inLife;
layout(location = 3) in float inType;

out float vLife;
out float vType;
out float vDist;
out vec2 vVelScreen;

float hash11(float p)
{
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main()
{
    vLife = inLife;
    vType = inType;

    vec3 pos = inPos;
    if (inType > 0.5) {
        float h1 = hash11(float(gl_VertexID) * 0.123);
        pos.x += sin(uTime * 1.2 + h1 * 6.28) * 0.4;
    }

    vec4 clipPos = uViewProj * vec4(pos, 1.0);
    gl_Position = clipPos;
    vDist = clipPos.w;

    // Project velocity for rain streaks
    vec4 clipRef = uViewProj * vec4(uPlayerPos, 1.0);
    vec4 clipNext = uViewProj * vec4(uPlayerPos + inVel * 100.0, 1.0);
    vVelScreen = (clipNext.xy / clipNext.w) - (clipRef.xy / clipRef.w);

    if (inType < 0.5) {
        gl_PointSize = 32.0;
    } else {
        gl_PointSize = 10.0;
    }
}
