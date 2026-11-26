// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform mat4 uMVP;
layout(std140) uniform NamedColorsBlock
{
    vec4 uNamedColors[MAX_NAMED_COLORS];
};

// Explore-mode fog. When uFogEnabled is 0 nothing below is evaluated.
uniform int uFogEnabled;
uniform ivec4 uFogMin;
uniform ivec4 uFogSize;
uniform sampler2DArray uFogMask;

// Secret exits the character has found. A quad tagged in bits 16-20 of aVertTexCol.w (see
// RoomQuadTag.h) is one form of a secret exit: 1 = door form, 2 = wall form, direction in bits
// 18-20. The door form is drawn only where the cell's bit for that direction is set, the wall
// form only where it is not. Without the texture nothing is found, so a secret stays a wall.
uniform int uFoundEnabled;
uniform ivec4 uFoundMin;
uniform ivec4 uFoundSize;
uniform usampler2DArray uFoundMask;

layout(location = 0) in ivec4 aVertTexCol;

out vec4 vColor;
out vec3 vTexCoord;

void main()
{
#if 0
    // GL_TRIANGLE_FAN
    //  3--2
    //  | /|  Triangles 012 and 023 both use CCW order.
    //  |/ |  Notice that the four vertices are in CCW order.
    //  0--1
    const ivec3 ioffsets[4] = ivec3[4](ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 1, 0), ivec3(0, 1, 0)); // fan
#else
    // GL_TRIANGLE_STRIP
    //  2--3  Note: Triangle strips alternate CCW/CW winding on every other triangle. This means...
    //  |\ |    triangle 012 is CCW order, but triangle 123 is CW order (backwards).
    //  | \|  Keep in mind that OpenGL does not actually draw the triangle backwards,
    //  0--1    so it does not affect glFrontFace() or the gl_FrontFacing variable.
    const ivec3 ioffsets[4] = ivec3[4](ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(0, 1, 0), ivec3(1, 1, 0)); // strip
#endif
    ivec3 ioffset = ioffsets[gl_VertexID];

    if (uFogEnabled != 0) {
        // One instance is one room cell, so the whole quad is kept or dropped together.
        // A cell outside the mask is hidden, and so is any cell that is not fully shown
        // (hinted cells are hidden until frontier hints exist).
        ivec3 cell = aVertTexCol.xyz - uFogMin.xyz;
        bool shown = all(greaterThanEqual(cell, ivec3(0))) && all(lessThan(cell, uFogSize.xyz));
        if (shown) {
            shown = texelFetch(uFogMask, cell, 0).r > 0.99;
        }
        if (!shown) {
            vColor = vec4(0.0);
            vTexCoord = vec3(0.0);
            // Outside the clip volume and degenerate: no fragments are generated.
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }
    }

    int secretForm = (aVertTexCol.w >> 16) & 3;
    if (secretForm != 0) {
        bool found = false;
        if (uFoundEnabled != 0) {
            ivec3 fcell = aVertTexCol.xyz - uFoundMin.xyz;
            if (all(greaterThanEqual(fcell, ivec3(0))) && all(lessThan(fcell, uFoundSize.xyz))) {
                uint bits = texelFetch(uFoundMask, fcell, 0).r;
                found = ((bits >> uint((aVertTexCol.w >> 18) & 7)) & 1u) != 0u;
            }
        }
        // 1 is the door form, which is drawn only once the secret is found.
        if ((secretForm == 1) != found) {
            vColor = vec4(0.0);
            vTexCoord = vec3(0.0);
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }
    }

    int texZ = aVertTexCol.w & 0xFF;
    // Bits 16 and up are a secret exit tag (see RoomQuadTag.h); they must not reach the color.
    int colorId = ((aVertTexCol.w >> 8) & 0xFF) % MAX_NAMED_COLORS;

    vColor = uNamedColors[colorId];
    vTexCoord = vec3(ioffset.xy, float(texZ));
    gl_Position = uMVP * vec4(aVertTexCol.xyz + ioffset, 1.0);
}
