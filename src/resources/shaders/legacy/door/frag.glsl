// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform vec4 uColor;

in float vAlpha;

out vec4 fColor;

void main()
{
    fColor = vec4(uColor.rgb, uColor.a * vAlpha);
}
