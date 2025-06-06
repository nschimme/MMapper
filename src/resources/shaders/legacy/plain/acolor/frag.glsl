#version 330 core
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform vec4 uColor;

in vec4 gs_color; // Received from Geometry Shader

out vec4 fragColor;

void main()
{
    fragColor = gs_color * uColor; // Modulate with uColor as per original logic
}
