// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2022 The MMapper Authors

#include "OpenGLTypes.h"
#include "legacy/Legacy.h"

IRenderable::~IRenderable() = default;

void GLRenderState::apply(Legacy::Functions &gl) const
{
    gl.applyRenderState(*this);
}

TexturedRenderable::TexturedRenderable(MMTextureId tex, std::unique_ptr<IRenderable> mesh)
    : m_texture{tex}
    , m_mesh{std::move(mesh)}
{}

TexturedRenderable::~TexturedRenderable() = default;

void TexturedRenderable::virt_clear()
{
    m_mesh->clear();
}

void TexturedRenderable::virt_reset()
{
    m_mesh->reset();
}

bool TexturedRenderable::virt_isEmpty() const
{
    return m_mesh->isEmpty();
}

void TexturedRenderable::virt_render(const GLRenderState &renderState)
{
    m_mesh->render(renderState.withTexture0(m_texture));
}
