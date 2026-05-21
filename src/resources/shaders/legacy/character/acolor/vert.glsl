// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uMVP;

layout(std140) uniform TimeBlock
{
    vec4 uTime; // x=time, y=delta, zw=unused
};

layout(location = 0) in vec4 aColor;
layout(location = 1) in vec3 aVert;
layout(location = 2) in vec3 aOldVert;
layout(location = 3) in float aStartTime;

out vec4 vColor;

void main()
{
    float t = clamp((uTime.x - aStartTime) / 0.060, 0.0, 1.0);
    vec3 pos = mix(aOldVert, aVert, t);
    vColor = aColor;
    gl_Position = uMVP * vec4(pos, 1.0);
}
