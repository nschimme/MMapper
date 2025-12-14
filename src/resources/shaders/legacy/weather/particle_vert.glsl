// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

uniform mat4 uMVP;
uniform float uTime;

layout(location = 0) in vec3 aStartPosition;
layout(location = 1) in vec3 aVelocity;

void main()
{
    float y = aStartPosition.y + aVelocity.y * uTime;
    vec3 position = vec3(aStartPosition.x, mod(y, 200.0), aStartPosition.z);
    gl_Position = uMVP * vec4(position, 1.0);
    gl_PointSize = 2.0;
}
