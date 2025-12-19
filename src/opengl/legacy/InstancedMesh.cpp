// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "InstancedMesh.h"

namespace Legacy {

InstancedMesh::InstancedMesh(const SharedFunctions& functions)
    : m_functions(functions)
{
    m_vao.emplace(functions);
    m_vbo.emplace(functions);
}

InstancedMesh::~InstancedMesh() = default;

void InstancedMesh::virt_clear()
{
    m_instanceCount = 0;
}

void InstancedMesh::virt_reset()
{
    m_vbo.reset();
    m_vao.reset();
    m_instanceCount = 0;
}

bool InstancedMesh::virt_isEmpty() const
{
    return m_instanceCount == 0;
}

void InstancedMesh::virt_render(const GLRenderState& renderState)
{
    if (virt_isEmpty()) {
        return;
    }

    Functions& gl = *m_functions;
    gl.glBindVertexArray(m_vao.get());
    virt_bind_attributes();
    gl.glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, m_instanceCount);
    gl.glBindVertexArray(0);
}

} // namespace Legacy
