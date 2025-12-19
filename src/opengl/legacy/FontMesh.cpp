// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "FontMesh.h"
#include "Legacy.h"

#define VOIDPTR_OFFSETOF(x, y) reinterpret_cast<void *>(offsetof(x, y))
#define VPO(x) VOIDPTR_OFFSETOF(FontData, x)

namespace Legacy {

FontMesh::FontMesh(const SharedFunctions& functions, const std::shared_ptr<FontShader>& sharedShader)
    : InstancedMesh(functions)
    , m_shader(sharedShader)
{
}

FontMesh::~FontMesh() = default;

void FontMesh::update(const std::vector<FontData>& fontData)
{
    m_instanceCount = fontData.size();
    if (m_instanceCount == 0) {
        return;
    }

    Functions& gl = *m_functions;
    gl.glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
    gl.glBufferData(GL_ARRAY_BUFFER, fontData.size() * sizeof(FontData), fontData.data(), GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FontMesh::virt_bind_attributes()
{
    Functions& gl = *m_functions;

    const auto pos = m_shader->getAttribLocation("aPos");
    const auto size = m_shader->getAttribLocation("aSize");
    const auto texTopLeft = m_shader->getAttribLocation("aTexTopLeft");
    const auto texBottomRight = m_shader->getAttribLocation("aTexBottomRight");
    const auto color = m_shader->getAttribLocation("aColor");
    const auto italics = m_shader->getAttribLocation("aItalics");
    const auto rotation = m_shader->getAttribLocation("aRotation");

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
    const auto stride = static_cast<GLsizei>(sizeof(FontData));

    gl.enableAttrib(pos, 3, GL_FLOAT, GL_FALSE, stride, VPO(pos));
    gl.glVertexAttribDivisor(pos, 1);

    gl.enableAttrib(size, 2, GL_FLOAT, GL_FALSE, stride, VPO(size));
    gl.glVertexAttribDivisor(size, 1);

    gl.enableAttrib(texTopLeft, 2, GL_FLOAT, GL_FALSE, stride, VPO(texTopLeft));
    gl.glVertexAttribDivisor(texTopLeft, 1);

    gl.enableAttrib(texBottomRight, 2, GL_FLOAT, GL_FALSE, stride, VPO(texBottomRight));
    gl.glVertexAttribDivisor(texBottomRight, 1);

    gl.enableAttrib(color, 4, GL_FLOAT, GL_FALSE, stride, VPO(color));
    gl.glVertexAttribDivisor(color, 1);

    gl.enableAttrib(italics, 1, GL_FLOAT, GL_FALSE, stride, VPO(italics));
    gl.glVertexAttribDivisor(italics, 1);

    gl.enableAttrib(rotation, 1, GL_FLOAT, GL_FALSE, stride, VPO(rotation));
    gl.glVertexAttribDivisor(rotation, 1);

    gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace Legacy
