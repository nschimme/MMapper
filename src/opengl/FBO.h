#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <memory>

#include <QOpenGLFramebufferObject>
#include <QSize>

namespace Legacy {
class Functions;
}

class FBO final
{
public:
    explicit FBO();
    ~FBO();
    FBO(const FBO &) = delete;
    FBO &operator=(const FBO &) = delete;

public:
    void configure(const QSize &size, int samples, float devicePixelRatio);
    void bind();
    void release();
    void blitToDefault();

private:
    std::unique_ptr<QOpenGLFramebufferObject> m_multisamplingFbo;
    std::unique_ptr<QOpenGLFramebufferObject> m_resolvedFbo;
};
