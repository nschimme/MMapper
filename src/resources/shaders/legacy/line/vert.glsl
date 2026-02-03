// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform mat4 uMVP;
uniform ivec4 uViewport;

layout(location = 0) in vec4 aColor;
layout(location = 1) in vec3 aFrom;
layout(location = 2) in vec3 aTo;
layout(location = 3) in float aWidth;

out vec4 vColor;
out float isoLinePos;

vec2 toScreenSpace(vec4 v)
{
    vec3 ndc = v.xyz / v.w;
    return (ndc.xy * 0.5 + 0.5) * vec2(uViewport.zw);
}

void main()
{
    vColor = aColor;

    vec2 p1 = toScreenSpace(uMVP * vec4(aFrom, 1.0));
    vec2 p2 = toScreenSpace(uMVP * vec4(aTo, 1.0));
    vec2 dir = normalize(p2 - p1);
    vec2 normal = vec2(-dir.y, dir.x);
    float halfWidth = aWidth * 0.5;
    vec2 offset = normal * halfWidth;

    vec2 positions[4] = vec2[](p1 - offset, p1 + offset, p2 - offset, p2 + offset);

    vec2 pos = positions[gl_VertexID];
    // to NDC
    pos = (pos / vec2(uViewport.zw)) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);

    float weights[4] = float[](-1.0, 1.0, -1.0, 1.0);
    isoLinePos = weights[gl_VertexID];
}
