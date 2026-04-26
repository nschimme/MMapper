// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform sampler2D uTexture;

in vec2 vTexCoord;

out vec4 vFragmentColor;

void main()
{
    // Note: This shader is intended for blitting the internal FBO to the default
    // framebuffer. It forces the alpha channel to 1.0 to ensure the window is
    // fully opaque, which helps prevent ghosting issues on Wayland.
    vFragmentColor = vec4(texture(uTexture, vTexCoord).rgb, 1.0);
}
