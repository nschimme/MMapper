// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform vec4 uColor;

layout(location = 0) out vec4 out_FragColor; // Added modern output

void main()
{
    out_FragColor = uColor; // Changed to out_FragColor
}
