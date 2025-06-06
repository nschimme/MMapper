// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform vec4 uColor;

in vec4 gs_color; // Received from Geometry Shader. Changed from 'varying' to 'in'

layout(location = 0) out vec4 out_FragColor; // Added explicit modern output

void main()
{
    out_FragColor = gs_color * uColor; // Changed from 'gl_FragColor' to 'out_FragColor'
}
