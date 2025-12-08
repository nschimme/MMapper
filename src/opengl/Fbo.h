#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <memory>

#include <QOpenGLFramebufferObject>
#include <QSize>

namespace Legacy {
class Functions;
}

class Fbo final
{
public:
    explicit Fbo();
    ~Fbo();
    Fbo(const Fbo &) = delete;
    Fbo &operator=(const Fbo &) = delete;

public:
    void configure(const QSize &size, int samples, float devicePixelRatio, Legacy::Functions &gl);
    void bind();
    void release();
    void blitToDefault();

private:
    std::unique_ptr<QOpenGLFramebufferObject> m_multisamplingFbo;
    std::unique_ptr<QOpenGLFramebufferObject> m_resolvedFbo;
};
