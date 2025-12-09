// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

in vec4 vColor;
in vec2 vLineCoord;

out vec4 vFragmentColor;

void main()
{
    // vLineCoord.y ranges from -1 to 1 across the line width.
    // Calculate distance from the center of the line (0.0).
    float dist = abs(vLineCoord.y);

    // Calculate the screen-space derivative of the distance.
    // This gives us a measure of how much `dist` changes per pixel.
    float fw = fwidth(dist);

    // Use smoothstep to create a soft edge for anti-aliasing.
    // The line is fully opaque when `dist` is less than `1.0 - fw`
    // and fades to transparent as it approaches `1.0`.
    float alpha = 1.0 - smoothstep(1.0 - fw, 1.0, dist);

    // Combine with vertex color's alpha.
    vFragmentColor = vec4(vColor.rgb, vColor.a * alpha);
}
