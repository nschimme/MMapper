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
    gl.glBufferData(GL_ARRAY_BUFFER, static_cast<qopengl_GLsizeiptr>(fontData.size() * sizeof(FontData)), fontData.data(), GL_DYNAMIC_DRAW);
    gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FontMesh::cacheAttributeLocations() {
    if (m_posAttr.has_value()) {
        return;
    }
    m_posAttr = static_cast<GLint>(m_shader->getAttribLocation("aPos"));
    m_sizeAttr = static_cast<GLint>(m_shader->getAttribLocation("aSize"));
    m_texTopLeftAttr = static_cast<GLint>(m_shader->getAttribLocation("aTexTopLeft"));
    m_texBottomRightAttr = static_cast<GLint>(m_shader->getAttribLocation("aTexBottomRight"));
    m_colorAttr = static_cast<GLint>(m_shader->getAttribLocation("aColor"));
    m_italicsAttr = static_cast<GLint>(m_shader->getAttribLocation("aItalics"));
    m_rotationAttr = static_cast<GLint>(m_shader->getAttribLocation("aRotation"));
}

void FontMesh::virt_bind_attributes()
{
    Functions& gl = *m_functions;

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
    const auto stride = static_cast<GLsizei>(sizeof(FontData));

    gl.enableAttrib(static_cast<GLuint>(m_posAttr.value()), 4, GL_FLOAT, GL_FALSE, stride, VPO(pos));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_posAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_sizeAttr.value()), 2, GL_FLOAT, GL_FALSE, stride, VPO(size));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_sizeAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_texTopLeftAttr.value()), 2, GL_FLOAT, GL_FALSE, stride, VPO(texTopLeft));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_texTopLeftAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_texBottomRightAttr.value()), 2, GL_FLOAT, GL_FALSE, stride, VPO(texBottomRight));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_texBottomRightAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_colorAttr.value()), 4, GL_FLOAT, GL_FALSE, stride, VPO(color));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_colorAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_italicsAttr.value()), 1, GL_FLOAT, GL_FALSE, stride, VPO(italics));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_italicsAttr.value()), 1);

    gl.enableAttrib(static_cast<GLuint>(m_rotationAttr.value()), 1, GL_FLOAT, GL_FALSE, stride, VPO(rotation));
    gl.glVertexAttribDivisor(static_cast<GLuint>(m_rotationAttr.value()), 1);

    gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FontMesh::virt_render(const GLRenderState& renderState)
{
    auto unbinder = m_shader->bind();
    cacheAttributeLocations();
    m_shader->setUniforms(m_functions->getProjectionMatrix(), renderState.uniforms);
    draw(renderState);
}

} // namespace Legacy
