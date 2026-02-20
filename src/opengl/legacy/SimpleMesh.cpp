// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "SimpleMesh.h"

#include "../../global/ConfigConsts.h"
#include "../SharedBufferManager.h"

void Legacy::drawRoomQuad(Functions &gl, const GLsizei numVerts)
{
    static constexpr size_t NUM_ELEMENTS = 4;
    gl.getSharedBufferManager().bind(gl, SharedVboEnum::InstancedQuadIbo);

    gl.glDrawElementsInstanced(GL_TRIANGLE_FAN, NUM_ELEMENTS, GL_UNSIGNED_BYTE, nullptr, numVerts);

    gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
