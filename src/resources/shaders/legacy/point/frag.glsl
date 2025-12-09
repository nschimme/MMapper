// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019-2025 The MMapper Authors

in vec4 vColor;

out vec4 vFragmentColor;

void main()
{
    // Calculate distance from the center of the point
    float dist = length(gl_PointCoord - vec2(0.5));

    // Use fwidth to get a screen-space derivative for anti-aliasing
    float fw = fwidth(dist);

    // Use smoothstep for a soft, anti-aliased edge
    float alpha = 1.0 - smoothstep(0.5 - fw, 0.5, dist);

    // Combine with vertex color's alpha
    vFragmentColor = vec4(vColor.rgb, vColor.a * alpha);
}
