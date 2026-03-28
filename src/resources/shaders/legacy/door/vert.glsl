// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uMVP;

layout(location = 0) in vec3 aVert;
layout(location = 1) in uint aRoomId;
layout(location = 2) in uint aDirection;

out float vAlpha;

struct DoorEntry {
    uint roomId;
    uint direction;
    uint state;
    uint unused;
};

layout(std140) uniform DoorBlock {
    DoorEntry doors[100];
} uDoors;

void main()
{
    vAlpha = 1.0;
    for (int i = 0; i < 100; ++i) {
        if (uDoors.doors[i].roomId == 0u) break;
        if (uDoors.doors[i].roomId == aRoomId && uDoors.doors[i].direction == aDirection) {
            // State: 0=CLOSED, 1=OPEN, 2=BROKEN
            // Open or broken doors are mostly transparent
            if (uDoors.doors[i].state != 0u) {
                vAlpha = 0.2;
            }
            break;
        }
    }
    gl_Position = uMVP * vec4(aVert, 1.0);
}
