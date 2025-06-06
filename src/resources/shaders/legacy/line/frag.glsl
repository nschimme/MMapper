// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

uniform vec4 uColor; // Modulation color, standard in project's shaders

in vec4 vColor;       // Interpolated color from vertex shader (derived from aInstanceColor)

out vec4 vFragmentColor;

void main() {
    // Modulate the instance's color (vColor) with the uniform uColor.
    // If uColor is white (1,1,1,1), vFragmentColor will be same as vColor.
    vFragmentColor = vColor * uColor;
}
