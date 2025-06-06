// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

uniform sampler2D uFontTexture;

in vec4 vColor; // Changed from varying
in vec2 vTexCoord; // Changed from varying

layout(location = 0) out vec4 out_FragColor; // Added modern output

void main()
{
    out_FragColor = vColor * texture2D(uFontTexture, vTexCoord); // Changed to out_FragColor
}
