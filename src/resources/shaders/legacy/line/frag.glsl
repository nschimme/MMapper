// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform vec4 uColor;

in vec4 vColor;
in float isoLinePos;

out vec4 vFragmentColor;

void main()
{
    float x = 1.0 - abs(isoLinePos);
    x = smoothstep(0.0, 1.0, x);
    vFragmentColor = vColor * uColor;
    vFragmentColor.a *= x;
}
