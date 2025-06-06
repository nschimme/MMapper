// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform sampler2D uTexture;
uniform vec4 uColor;

in vec2 vTexCoord; // Changed from varying

layout(location = 0) out vec4 out_FragColor; // Added modern output

void main()
{
    out_FragColor = uColor * texture(uTexture, vTexCoord); // Changed to out_FragColor
}
