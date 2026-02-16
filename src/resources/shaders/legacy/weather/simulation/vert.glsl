// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

layout(location = 0) in vec2 aPos;
layout(location = 1) in float aHash;
layout(location = 2) in float aType;

uniform float uDeltaTime;
uniform vec2 uPlayerPos;

out vec2 vPos;
out float vHash;
out float vType;

void main()
{
    float hashVal = aHash;
    vHash = aHash;
    vType = aType;
    vec2 pos = aPos;

    if (aType == 0.0) { // Rain
        float speed = 20.0 + hashVal * 5.0;
        pos.y -= uDeltaTime * speed;
    } else { // Snow
        pos.y -= uDeltaTime * 2.0;
    }

    // Toroidal wrap around player in a 40x40 box
    // This ensures we always have particles around the player
    vec2 rel = pos - uPlayerPos;
    if (rel.x < -20.0)
        pos.x += 40.0;
    else if (rel.x > 20.0)
        pos.x -= 40.0;

    if (rel.y < -20.0)
        pos.y += 40.0;
    else if (rel.y > 20.0)
        pos.y -= 40.0;

    vPos = pos;
    gl_Position = vec4(vPos, 0.0, 1.0);
}
