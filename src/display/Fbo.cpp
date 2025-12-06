// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "Fbo.h"

#include "../opengl/legacy/Legacy.h"

#include <QDebug>

struct Fbo::State
{
    Legacy::WeakFunctions weakFunctions;
    int width = 0;
    int height = 0;
    GLuint fboId = 0;
    GLuint colorRenderbufferId = 0;
    GLuint depthRenderbufferId = 0;
};

Fbo::Fbo() = default;

Fbo::~Fbo()
{
    reset();
}

void Fbo::emplace(const Legacy::SharedFunctions &sharedFunctions, int width, int height, int samples)
{
    if (m_state) {
        reset();
    }
    m_state = std::make_unique<State>();
    m_state->weakFunctions = sharedFunctions;
    m_state->width = width;
    m_state->height = height;

    auto &gl = *sharedFunctions;

    gl.glGenFramebuffers(1, &m_state->fboId);
    gl.glBindFramebuffer(GL_FRAMEBUFFER, m_state->fboId);

    gl.glGenRenderbuffers(1, &m_state->colorRenderbufferId);
    gl.glBindRenderbuffer(GL_RENDERBUFFER, m_state->colorRenderbufferId);
    gl.glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                 GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER,
                                 m_state->colorRenderbufferId);

    gl.glGenRenderbuffers(1, &m_state->depthRenderbufferId);
    gl.glBindRenderbuffer(GL_RENDERBUFFER, m_state->depthRenderbufferId);
    gl.glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
    gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                 GL_DEPTH_ATTACHMENT,
                                 GL_RENDERBUFFER,
                                 m_state->depthRenderbufferId);

    if (gl.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        qWarning() << "Failed to create multisampled FBO";
        reset();
    }

    gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Fbo::reset()
{
    if (!m_state) {
        return;
    }

    if (auto sharedFunctions = m_state->weakFunctions.lock()) {
        auto &gl = *sharedFunctions;
        if (m_state->fboId) {
            gl.glDeleteFramebuffers(1, &m_state->fboId);
        }
        if (m_state->colorRenderbufferId) {
            gl.glDeleteRenderbuffers(1, &m_state->colorRenderbufferId);
        }
        if (m_state->depthRenderbufferId) {
            gl.glDeleteRenderbuffers(1, &m_state->depthRenderbufferId);
        }
    }
    m_state.reset();
}

void Fbo::bind()
{
    if (m_state) {
        if (auto sharedFunctions = m_state->weakFunctions.lock()) {
            sharedFunctions->glBindFramebuffer(GL_FRAMEBUFFER, m_state->fboId);
        }
    }
}

void Fbo::release()
{
    if (m_state) {
        if (auto sharedFunctions = m_state->weakFunctions.lock()) {
            sharedFunctions->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}

void Fbo::blit()
{
    if (m_state) {
        if (auto sharedFunctions = m_state->weakFunctions.lock()) {
            auto &gl = *sharedFunctions;
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, m_state->fboId);
            gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            gl.glBlitFramebuffer(0,
                                 0,
                                 m_state->width,
                                 m_state->height,
                                 0,
                                 0,
                                 m_state->width,
                                 m_state->height,
                                 GL_COLOR_BUFFER_BIT,
                                 GL_NEAREST);
            gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
}

GLuint Fbo::getId() const
{
    return m_state ? m_state->fboId : 0;
}

Fbo::operator bool() const
{
    return m_state && m_state->fboId != 0;
}
