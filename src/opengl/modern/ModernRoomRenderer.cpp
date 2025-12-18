// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "ModernRoomRenderer.h"
#include "../OpenGL.h"
#include "RoomInstanceData.h"
#include "../legacy/Shaders.h"
#include "../legacy/RoomShader.h"

namespace modern
{

class RoomRenderer::Private
{
public:
    Private(OpenGL &gl, const std::vector<RoomInstanceData> &instances)
        : m_gl(gl)
        , m_shader(gl.getFunctions().getShaderPrograms().getRoomShader())
        , m_vao(0)
        , m_vbo(0)
        , m_instance_count(instances.size())
    {
        // Create a VAO
        gl.getFunctions().glGenVertexArrays(1, &m_vao);
        gl.getFunctions().glBindVertexArray(m_vao);

        // Create a VBO for the instance data
        gl.getFunctions().glGenBuffers(1, &m_vbo);
        gl.getFunctions().glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        gl.getFunctions().glBufferData(GL_ARRAY_BUFFER,
                                     instances.size() * sizeof(RoomInstanceData),
                                     instances.data(),
                                     GL_STATIC_DRAW);

        // Set up vertex attributes
        m_shader->enableAttributes(gl);

        // Unbind
        gl.getFunctions().glBindVertexArray(0);
        gl.getFunctions().glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    ~Private()
    {
        m_gl.getFunctions().glDeleteBuffers(1, &m_vbo);
        m_gl.getFunctions().glDeleteVertexArrays(1, &m_vao);
    }

    void render()
    {
        auto unbinder = m_shader->bind();
        m_shader->setProjection(m_gl.getProjectionMatrix());
        m_shader->setUniform1iv(m_shader->getUniformLocation("u_texture"), 1, &m_texture_unit);

        m_gl.getFunctions().glActiveTexture(GL_TEXTURE0 + m_texture_unit);
        m_gl.getFunctions().glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture_id.value());

        m_gl.getFunctions().glBindVertexArray(m_vao);
        m_gl.getFunctions().glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, m_instance_count);
        m_gl.getFunctions().glBindVertexArray(0);
    }

public:
    OpenGL &m_gl;
    std::shared_ptr<Legacy::RoomShader> m_shader;
    GLuint m_vao;
    GLuint m_vbo;
    size_t m_instance_count;
    MMTextureId m_texture_id;
    GLint m_texture_unit = 0;
};

RoomRenderer::RoomRenderer(OpenGL &gl, const std::vector<RoomInstanceData> &instances)
    : d(std::make_unique<Private>(gl, instances))
{}

RoomRenderer::~RoomRenderer() = default;

void RoomRenderer::setTexture(MMTextureId textureId)
{
    d->m_texture_id = textureId;
}

void RoomRenderer::virt_clear()
{
    // Not implemented yet
}

void RoomRenderer::virt_reset()
{
    // Not implemented yet
}

bool RoomRenderer::virt_isEmpty() const
{
    return d->m_instance_count == 0;
}

void RoomRenderer::virt_render(const GLRenderState &renderState)
{
    d->render();
}

} // namespace modern
