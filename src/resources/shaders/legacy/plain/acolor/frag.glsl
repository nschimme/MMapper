// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform vec4 uColor;

in vec4 vs_color_out; // Renamed from gs_color, Received from Geometry/Vertex Shader.

layout(location = 0) out vec4 out_FragColor; // Added explicit modern output

void main()
{
    out_FragColor = vs_color_out * uColor; // Changed from gs_color to vs_color_out
}
