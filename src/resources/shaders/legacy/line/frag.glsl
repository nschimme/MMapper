// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

in vec4 vColor;
in vec2 vStipple;
in float vStipplePos;
in float vLineLength;

out vec4 fColor;

void main() {
    fColor = vColor;

    if (vStipple.x > 0.0) {
        float stippleFactor = vStipple.x;
        float stipplePattern = vStipple.y;

        float stippleCoord = (vStipplePos + vLineLength * 0.5) / stippleFactor;
        float stippleBit = floor(mod(stippleCoord, 16.0));
        if ((uint(stipplePattern) & (1u << uint(stippleBit))) == 0u) {
            discard;
        }
    }
}
